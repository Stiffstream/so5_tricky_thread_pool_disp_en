#pragma once

#include <common/args.hpp>

#include <clara/clara.hpp>

#include <fmt/format.h>

#include <variant>

struct help_requested_t {};

inline std::variant<help_requested_t, args_t>
parse_args(int argc, char ** argv) {
   auto device_count = args_t::default_device_count;
   auto thread_pool_size = args_t::default_thread_pool_size;
   auto io_ops_before_reinit = args_t::default_io_ops_defore_reinit;
   auto reinits_before_recreate = args_t::default_reinits_before_recreate;

   auto io_ops_period_left = io_ops_period_range_t::default_left.count();
   auto io_ops_period_right = io_ops_period_range_t::default_right.count();

   auto device_init_time = args_t::default_device_init_time.count();
   auto io_op_time = args_t::default_io_op_time.count();

   bool help_requested = false;

   // Подготавливаем парсер аргументов командной строки.
   using namespace clara;

   auto cli = Opt(device_count, "device_count")["-d"]["--device-count"]
            ("count of devices, default: " + std::to_string(device_count))
      | Opt(thread_pool_size, "pool_size")["-t"]["--thread-pool"]
            (fmt::format("count of threads in poll, default: {}",
               thread_pool_size))
      | Opt(io_ops_before_reinit, "ops_count")
            ["-I"]["--io-ops-before-reinit"]
            (fmt::format("max IO operations before device reinit, default: {}",
               io_ops_before_reinit))
      | Opt(reinits_before_recreate, "reinits_count")
            ["-R"]["--reinits-before-recreate"]
            (fmt::format("max reinit operations before recreation of device, default: {}",
               reinits_before_recreate))
      | Opt(io_ops_period_left, "ms")
            ["-m"]["--io-ops-time-min"]
            (fmt::format("minimal IO operation time (milliseconds), default: {}",
               io_ops_period_left))
      | Opt(io_ops_period_right, "ms")
            ["-M"]["--io-ops-time-max"]
            (fmt::format("maximum IO operation time (milliseconds), default: {} ",
               io_ops_period_right))
      | Opt(device_init_time, "ms")
            ["-i"]["--init-time"]
            (fmt::format("device init time (milliseconds), default: {}",
               device_init_time))
      | Opt(io_op_time, "ms")
            ["-o"]["--io-op-time"]
            (fmt::format("device IO-operation time (milliseconds), default: {}",
               io_op_time))
      | Help(help_requested);

   // Выполняем парсинг...
   auto parse_result = cli.parse(Args(argc, argv));
   // ...и бросаем исключение если столкнулись с ошибкой.
   if(!parse_result)
      throw std::runtime_error("Invalid command line: "
            + parse_result.errorMessage());

   if(help_requested) {
      std::cout << cli << std::endl;
      return help_requested_t{};
   }
   else {
      const auto min_value_checker =
            [](const auto v, const auto min, const char * name) {
               const auto actual_min = static_cast<decltype(v)>(min);
               if(v < actual_min)
                  throw std::invalid_argument(
                        fmt::format("minimal allowed value for {} is {}",
                              name, actual_min));
            };

      min_value_checker(device_count, 1, "device_count");
      min_value_checker(thread_pool_size, 2, "thread_pool_size");
      min_value_checker(io_ops_before_reinit, 2, "io_ops_before_reinit");
      min_value_checker(reinits_before_recreate, 2, "reinits_before_recreate");

      min_value_checker(io_ops_period_left, 0, "io_ops_period min");
      min_value_checker(io_ops_period_right, 100, "io_ops_period max");
      if(io_ops_period_left >= io_ops_period_right)
         throw std::invalid_argument(
               "minimal value of io_ops_period must be less than maximum value");

      min_value_checker(device_init_time, 10, "device_init_time");
      min_value_checker(io_op_time, 10, "io_op_time");
   }

   return args_t{
         device_count,
         thread_pool_size,
         io_ops_before_reinit,
         reinits_before_recreate,
         io_ops_period_range_t{
            std::chrono::milliseconds{io_ops_period_left},
            std::chrono::milliseconds{io_ops_period_right} },
         std::chrono::milliseconds{device_init_time},
         std::chrono::milliseconds{io_op_time} };
}

