#include <iostream>
#include <boost/program_options.hpp>
#include <rembrandt/benchmark/throughput/kafka/producer.h>
#include <rembrandt/logging/throughput_logger.h>
#include <rembrandt/benchmark/common/rate_limiter.h>

void busy_polling(RdKafka::Producer *producer, std::atomic<bool> &running) 
{
  running = true;
  while (running) {
    producer->poll(1000);
  }
}

ThroughputKafkaProducer::ThroughputKafkaProducer(int argc, char *const *argv)
    : kconfig_p_(std::unique_ptr<RdKafka::Conf>(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL))),
      free_buffers_p_(std::make_unique<tbb::concurrent_bounded_queue<char *>>()),
      generated_buffers_p_(std::make_unique<tbb::concurrent_bounded_queue<char *>>()),
      counter_(0), 
      dr_cb_(counter_, *(free_buffers_p_.get()))
{
  const size_t kNumBuffers = 32;
  this->ParseOptions(argc, argv);
  this->ConfigureKafka();

  std::string errstr;
  producer_p_ = std::unique_ptr<RdKafka::Producer>(RdKafka::Producer::create(kconfig_p_.get(), errstr));
  if (!producer_p_)
  {
    std::cerr << "Failed to create producer: " << errstr << std::endl;
    exit(1);
  }

  for (size_t _ = 0; _ < kNumBuffers; _++)
  {
    // TODO: Improved RAII
    auto pointer = (char *)malloc(config_.max_batch_size);
    free_buffers_p_->push(pointer);
  }

  warmup_generator_p_ = ParallelDataGenerator::Create(config_.max_batch_size,
                                                      config_.rate_limit,
                                                      0,
                                                      1000,
                                                      5,
                                                      MODE::RELAXED);

  generator_p_ = ParallelDataGenerator::Create(config_.max_batch_size,
                                               config_.rate_limit,
                                               0,
                                               1000,
                                               5,
                                               MODE::RELAXED); // TODO: Adjust mode init
}

void ThroughputKafkaProducer::Run()
{
  std::cout << "Preparing run..." << std::endl;

  ThroughputLogger logger =
      ThroughputLogger(counter_, config_.log_directory, "benchmark_producer_throughput", config_.max_batch_size);
  char *buffer;

  std::atomic<bool> running = false;
  std::thread thread(busy_polling, producer_p_.get(), std::ref(running));

  std::cout << "Starting warmup execution..." << std::endl;
  warmup_generator_p_->Start(GetWarmupBatchCount(), *free_buffers_p_, *generated_buffers_p_);

  for (size_t count = 0; count < GetWarmupBatchCount(); count++)
  {
    if (count % (GetWarmupBatchCount() / 10) == 0)
    {
      std::cout << "Warmup Iteration: " << count << " / " << GetWarmupBatchCount() << std::endl;
    }
    generated_buffers_p_->pop(buffer);
    RdKafka::ErrorCode resp = producer_p_->produce(std::string("benchmark"), 0, RdKafka::Producer::RK_MSG_BLOCK, buffer, config_.max_batch_size, nullptr, 0, 0, nullptr, buffer);
    if (resp != RdKafka::ERR_NO_ERROR)
    {
      std::cerr << "Produce failed: " << RdKafka::err2str(resp) << std::endl;
      exit(1);
    }
    producer_p_->poll(0);
  }

  while (producer_p_->outq_len() > 0)
  {
    producer_p_->flush(1000);
  }
  
  if (counter_ < GetWarmupBatchCount())
  {
    std::cerr << "Failed to send all warmup messages: " << counter_ << " / " << GetWarmupBatchCount() << std::endl;
    exit(1);
  }
  std::cout << "Finished warmup!" << std::endl;
  warmup_generator_p_->Stop();

  std::cout << "Starting run execution..." << std::endl;
  generator_p_->Start(GetRunBatchCount(), *free_buffers_p_, *generated_buffers_p_);

  std::cout << "Starting logger..." << std::endl;
  counter_ = 0;
  logger.Start();

  auto start = std::chrono::high_resolution_clock::now();

  std::cout << "Starting run execution..." << std::endl;
  while (GetRunBatchCount() > 0 && generated_buffers_p_->size() == 0)
  {
  }
  for (size_t count = 0; count < GetRunBatchCount(); count++)
  {
    if (count % (GetRunBatchCount() / 10) == 0)
    {
      std::cout << "Iteration: " << count << " / " << GetRunBatchCount() << std::endl;
    }
    generated_buffers_p_->pop(buffer);
    RdKafka::ErrorCode resp = producer_p_->produce(std::string("benchmark"), 0, RdKafka::Producer::RK_MSG_BLOCK, buffer, config_.max_batch_size, nullptr, 0, 0, nullptr, buffer);
    if (resp != RdKafka::ERR_NO_ERROR) {
      std::cerr << "Produce failed: " << RdKafka::err2str(resp) << std::endl;
      exit(1);
    }
    producer_p_->poll(0);
  }

  while (producer_p_->outq_len() > 0)
  {
    producer_p_->flush(1000);
  }
  
  if (counter_ < GetRunBatchCount())
  {
    std::cerr << "Failed to send all messages: " << counter_ << " / " << GetRunBatchCount() << std::endl;
    exit(1);
  }
  std::cout << "Finishing run execution..." << std::endl;
  auto stop = std::chrono::high_resolution_clock::now();

  logger.Stop();
  std::cout << "Finished logger." << std::endl;
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  std::cout << "Duration: " << duration.count() << " ms" << std::endl;

  generator_p_->Stop();
  running = false;
  thread.join();
}

void ThroughputKafkaProducer::ParseOptions(int argc, char *const *argv)
{
  namespace po = boost::program_options;
  std::string mode_str;
  try
  {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("broker-node-ip",
         po::value(&config_.broker_node_ip)->default_value("10.150.1.12"),
         "IP address of the broker node")
        ("broker-node-port",
         po::value(&config_.broker_node_port)->default_value(13360),
         "Port number of the broker node")
        ("batch-size",
         po::value(&config_.max_batch_size)->default_value(131072),
         "Size of an individual batch (sending unit) in bytes")
        ("data-size",
         po::value(&config_.data_size)->default_value(config_.data_size),
        "Total amount of data transferred in this benchmark")
        ("warmup-fraction", po::value(&config_.warmup_fraction)->default_value(config_.warmup_fraction),
         "Fraction of data that is transferred during warmup")
        ("rate-limit", po::value(&config_.rate_limit)->default_value(config_.rate_limit),
         "The maximum amount of data that is transferred per second")
        ("log-dir",
         po::value(&config_.log_directory)->default_value("/hpi/fs00/home/hendrik.makait/rembrandt/logs/20200727/e2e/50/kafka/"),
         "Directory to store benchmark logs");
    po::variables_map variables_map;
    po::store(po::parse_command_line(argc, argv, desc), variables_map);
    po::notify(variables_map);

    if (variables_map.count("help"))
    {
      std::cout << "Usage: myExecutable [options]\n";
      std::cout << desc;
      exit(0);
    }
  }
  catch (const po::error &ex)
  {
    std::cout << ex.what() << std::endl;
    exit(1);
  }
}

void ThroughputKafkaProducer::ConfigureKafka()
{
  std::string errstr;
  if (kconfig_p_->set("dr_cb", &dr_cb_, errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  if (kconfig_p_->set("bootstrap.servers", config_.broker_node_ip, errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  if (kconfig_p_->set("linger.ms", "0", errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  if (kconfig_p_->set("batch.num.messages", "1`", errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  if (kconfig_p_->set("message.max.bytes", std::to_string(config_.max_batch_size * 1.5), errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  if (kconfig_p_->set("message.copy.max.bytes", "0", errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  if (kconfig_p_->set("acks", "all", errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }
}

size_t ThroughputKafkaProducer::GetBatchCount()
{
  return config_.data_size / config_.max_batch_size;
}

size_t ThroughputKafkaProducer::GetRunBatchCount()
{
  return GetBatchCount() - GetWarmupBatchCount();
}
size_t ThroughputKafkaProducer::GetWarmupBatchCount()
{
  return GetBatchCount() * config_.warmup_fraction;
}

int main(int argc, char *argv[])
{
  ThroughputKafkaProducer producer(argc, argv);
  producer.Run();
}