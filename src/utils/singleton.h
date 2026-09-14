#pragma once

template<typename T>
class Singleton {
public:
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;
	Singleton(Singleton&&) = delete;
	Singleton& operator=(Singleton&&) = delete;

	static T& Instance() {
		static T instance;
		return instance;
	}

protected:
	Singleton() = default;
};
