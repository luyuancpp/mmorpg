#include <boost/asio.hpp>
#include <memory>
#include <functional>
#include <chrono>

class TimerTaskBoostComp : public std::enable_shared_from_this<TimerTaskBoostComp> {
public:
	using TimerCallback = std::function<void()>;

	explicit TimerTaskBoostComp(boost::asio::io_context& io)
		: io_(io) {
	}

	~TimerTaskBoostComp() {
		Cancel();
	}

	void RunAt(std::chrono::steady_clock::time_point time, const TimerCallback& cb) {
		Cancel();
		callback_ = cb;
		timer_ = std::make_shared<boost::asio::steady_timer>(io_, time - std::chrono::steady_clock::now());
		timer_->async_wait([this, self = shared_from_this()](const boost::system::error_code& ec) {
			if (!ec) OnTimer();
			});
	}

	void RunAfter(double delaySeconds, const TimerCallback& cb) {
		RunAt(std::chrono::steady_clock::now() + std::chrono::milliseconds((int)(delaySeconds * 1000)), cb);
	}

	void RunEvery(double intervalSeconds, const TimerCallback& cb) {
		Cancel();
		interval_ = std::chrono::milliseconds((int)(intervalSeconds * 1000));
		callback_ = cb;
		timer_ = std::make_shared<boost::asio::steady_timer>(io_, interval_);
		timer_->async_wait([this, self = shared_from_this()](const boost::system::error_code& ec) {
			if (!ec) OnRecurringTimer();
			});
	}

	void Cancel() {
		if (timer_) {
			timer_->cancel();
			timer_.reset();
		}
		callback_ = nullptr;
	}

	bool IsActive() const {
		return timer_ != nullptr;
	}

	uint64_t GetEndTime() const {
		// Return expiration time as epoch seconds
		if (timer_) {
			auto tp = timer_->expiry();
			return std::chrono::duration_cast<std::chrono::seconds>(
				tp.time_since_epoch()).count();
		}
		return 0;
	}

	void SetCallback(const TimerCallback& cb) {
		callback_ = cb;
	}

private:
	void OnTimer() {
		if (callback_) {
			callback_();
		}
	}

	void OnRecurringTimer() {
		if (callback_) {
			callback_();
			timer_->expires_after(interval_);
			timer_->async_wait([this, self = shared_from_this()](const boost::system::error_code& ec) {
				if (!ec) OnRecurringTimer();
				});
		}
	}

	boost::asio::io_context& io_;
	std::shared_ptr<boost::asio::steady_timer> timer_;
	std::chrono::milliseconds interval_;
	TimerCallback callback_;
};
