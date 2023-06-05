#pragma once

namespace Clone {
	template<typename... Args>
	void info(Args&&... args);

	template<typename... Args>
	void danger(Args&&... args);

	template<typename... Args>
	void success(Args&&... args);

	template<typename... Args>
	void warn(Args&&... args);

	template<typename... Args>
	void debug(Args&&... args);
}

#include "logger.tpp"