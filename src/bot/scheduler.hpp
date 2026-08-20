#pragma once
#include <functional>
#include <unordered_map>

struct ScheduledJob {
    double m_interval;
    double m_lastExecuted = 0.0;

    bool m_repeat;
    bool m_stale = false;

    std::function<void()> m_executor;

    void update(float dt);
};

class BotScheduler {
   public:
    using JobExecutor = std::function<void()>;
    using JobId = uint64_t;

   private:
    JobId m_nextFreeId = 0;
    std::unordered_map<JobId, ScheduledJob> m_jobs;

   public:
    JobId schedule(const JobExecutor& executor, double interval,
                   bool repeat = false);
    void unschedule(JobId id);
    void reschedule(JobId id, double interval);

    void update(float dt);
};
