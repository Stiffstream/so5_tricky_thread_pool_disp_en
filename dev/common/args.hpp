#pragma once

#include <chrono>
#include <iostream>

// Диапазон значений для темпа операций ввода-вывода.
struct io_ops_period_range_t {
   static constexpr std::chrono::milliseconds default_left{ 100 };
   static constexpr std::chrono::milliseconds default_right{ 300 };

   std::chrono::milliseconds left_{ default_left };
   std::chrono::milliseconds right_{ default_right };
};

struct args_t {
   static constexpr unsigned default_device_count = 100u;
   static constexpr unsigned default_thread_pool_size = 4u;
   static constexpr unsigned default_io_ops_defore_reinit = 100u;
   static constexpr unsigned default_reinits_before_recreate = 10u;

   static constexpr std::chrono::milliseconds default_device_init_time{ 1250 };
   static constexpr std::chrono::milliseconds default_io_op_time{ 50 };

   // Количество устройств, которое нужно имитировать.
   unsigned device_count_{ default_device_count };

   // Количество рабочих потоков у диспетчера.
   unsigned thread_pool_size_{ default_thread_pool_size };

   // Максимальное количество операций ввода-вывода до переинициализации устройства.
   unsigned io_ops_before_reinit_{ default_io_ops_defore_reinit };
   // Максимальное количество переинициализаций перед пересозданием устройства.
   unsigned reinits_before_recreate_{ default_reinits_before_recreate };

   // Темп выполнения операций ввода-вывода.
   io_ops_period_range_t io_ops_period_;

   // Длительность операций инициализации и переинициализации устройств.
   std::chrono::milliseconds device_init_time_{ default_device_init_time };
   // Длительность операций ввода-вывода.
   std::chrono::milliseconds io_op_time_{ default_io_op_time };
};

inline void print_args(const args_t & a) {
   std::cout << "device_count: " << a.device_count_ << "\n"
      << "thread_pool_size: " << a.thread_pool_size_ << "\n"
      << "io_ops_before_reinit: " << a.io_ops_before_reinit_ << "\n"
      << "reinits_before_recreate: " << a.reinits_before_recreate_ << "\n"
      << "io_ops_period: [" << a.io_ops_period_.left_.count()
         << "," << a.io_ops_period_.right_.count() << "]\n"
      << "device_init_time: " << a.device_init_time_.count() << "ms\n"
      << "io_op_time: " << a.io_op_time_.count() << "ms"
      << std::endl;
};

