#pragma once

#include <so_5/all.hpp>

#include <fmt/ostream.h>

#include <fstream>

class a_dashboard_t final : public so_5::agent_t {
   struct show_stats_t final : public so_5::signal_t {};

public:
   // Type to be used for time counting.
   using clock_t = std::chrono::steady_clock;

   // Type of operation for that an information about the delay is related.
   enum class op_type_t : std::size_t { init = 0, io_op = 1, reinit = 2 };

   static constexpr std::size_t to_size_t(op_type_t v) {
      return static_cast<std::size_t>(v);
   }

   // A message with information about the time spent during
   // the delivery of a message.
   struct delay_info_t final : public so_5::message_t {
      op_type_t op_type_;
      clock_t::duration pause_;

      delay_info_t(op_type_t op_type, clock_t::duration pause)
         : op_type_(op_type), pause_(pause)
         {}
   };

   a_dashboard_t(context_t ctx) : so_5::agent_t(std::move(ctx)) {
      so_subscribe_self()
         .event(&a_dashboard_t::on_delay_info)
         .event(&a_dashboard_t::on_show_stats);
   }

   virtual void so_evt_start() override {
      // Initiate a periodic message for showing the current statistics.
      stats_timer_ = so_5::send_periodic<show_stats_t>(*this,
            std::chrono::milliseconds::zero(),
            std::chrono::seconds{5});

      // Make a csv-file for storing the current values.
      create_csv_file();
   }

private:
   struct time_slot_data_t {
      clock_t::duration total_time_{};
      std::uint_fast64_t total_events_{};

      time_slot_data_t & operator+=(const clock_t::duration d) {
         total_time_ += d;
         total_events_ += 1;
         return *this;
      }

      auto avg() const {
         const auto calc = [&]{ return total_time_ / total_events_; };
         decltype(calc()) r{};

         if(total_events_)
            r = calc();

         return r;
      }
   };

   struct event_data_t {
      time_slot_data_t total_;
      time_slot_data_t last_slot_;
   };

   std::array<event_data_t, static_cast<std::size_t>(op_type_t::reinit) + 1u> data_;

   so_5::timer_id_t stats_timer_;
   std::uint_fast64_t counter_{};

   // A file for storing the current values in csv-format.
   std::ofstream csv_file_;

   void on_delay_info(mhood_t<delay_info_t> cmd) {
      auto & d = data_[to_size_t(cmd->op_type_)];
      d.total_ += cmd->pause_;
      d.last_slot_ += cmd->pause_;
   }

   void on_show_stats(mhood_t<show_stats_t>) {
      store_current_data_to_csv_file();

      fmt::print("### === -- {} -- === ###\n", counter_);
      handle_stats_for(data_[to_size_t(op_type_t::init)], "init");
      handle_stats_for(data_[to_size_t(op_type_t::reinit)], "reinit");
      handle_stats_for(data_[to_size_t(op_type_t::io_op)], "io_op");
      fmt::print("\n");

      ++counter_;
   }

   void create_csv_file() {
      // Create a csv-file for storing the current values.
      // The current time in milliseconds will be used as the file name.
      using namespace std::chrono;

      csv_file_.exceptions(std::ofstream::badbit | std::ofstream::failbit);
      const auto file_name = fmt::format("{}.csv",
            duration_cast<milliseconds>(
                  steady_clock::now().time_since_epoch()).count());
      csv_file_.open(file_name);

      csv_file_ << "Init-Avg;Init-Cnt;Reinit-Avg;Reinit-Cnt;IO-Avg;IO-Cnt"
            << std::endl;
   }

   template<typename T>
   static auto ms(T v) {
      return std::chrono::duration_cast<std::chrono::milliseconds>(v).count();
   }

   void store_current_data_to_csv_file() {
      const auto & init = data_[to_size_t(op_type_t::init)];
      const auto & reinit = data_[to_size_t(op_type_t::reinit)];
      const auto & io_op = data_[to_size_t(op_type_t::io_op)];

      fmt::print(csv_file_,
            "{};{};{};{};{};{}\n",
            ms(init.last_slot_.avg()), init.last_slot_.total_events_,
            ms(reinit.last_slot_.avg()), reinit.last_slot_.total_events_,
            ms(io_op.last_slot_.avg()), io_op.last_slot_.total_events_);

      csv_file_.flush();
   }

   void handle_stats_for(
         event_data_t & data,
         const char * op_name) {
      fmt::print(
            "{:7}: total(avg)={:6}ms (events={:5}) | last(avg)={:6}ms (events={:5})\n",
            op_name,
            ms(data.total_.avg()), data.total_.total_events_,
            ms(data.last_slot_.avg()), data.last_slot_.total_events_);

      // Data for the last period should be dropped.
      data.last_slot_ = time_slot_data_t{};
   }
};

