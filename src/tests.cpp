#include "shared.h"

#include "d2d.h"

#include "shared/numeric_cast.h"

void test_floating_point_exceptions() {
	// _EM_ZERODIVIDE
	//float z = 0.0f;
	//assert(3.0f / z);

	// _EM_INVALID
	//assert(sqrt(-1.0f));

	// _EM_OVERFLOW
	//assert(std::numeric_limits<float>::max() * 2.0f);

	// _EM_UNDERFLOW
	//assert(std::numeric_limits<float>::min() / 2.0f);

	// _EM_DENORMAL
	// lowest priority. unclear how to cause without causing higher priority exceptions
}

void test_numeric_cast() {
	// char

	numeric_cast<unsigned char>(unsigned char(127));
	assert(ErrorTrap::is_good_and_reset());

	numeric_cast<signed char>(unsigned char(127));
	assert(ErrorTrap::is_good_and_reset());

	numeric_cast<unsigned char>(unsigned char(255));
	assert(ErrorTrap::is_good_and_reset());

	numeric_cast<signed char>(unsigned char(255));
	assert(!ErrorTrap::is_good_and_reset());

	numeric_cast<unsigned char>(signed char(-128));
	assert(!ErrorTrap::is_good_and_reset());

	numeric_cast<signed char>(signed char(-128));
	assert(ErrorTrap::is_good_and_reset());

	// int

	numeric_cast<int>(signed char(-128));
	assert(ErrorTrap::is_good_and_reset());

	numeric_cast<unsigned int>(signed char(-128));
	assert(!ErrorTrap::is_good_and_reset());

	numeric_cast<signed char>(-128);
	assert(ErrorTrap::is_good_and_reset());

	numeric_cast<unsigned char>(-128);
	assert(!ErrorTrap::is_good_and_reset());

	numeric_cast<signed char>(255);
	assert(!ErrorTrap::is_good_and_reset());

	numeric_cast<unsigned char>(255);
	assert(ErrorTrap::is_good_and_reset());

	numeric_cast<signed char>(256);
	assert(!ErrorTrap::is_good_and_reset());

	numeric_cast<unsigned char>(256);
	assert(!ErrorTrap::is_good_and_reset());

	// long long

	numeric_cast<int>(-1LL);
	assert(ErrorTrap::is_good_and_reset());

	numeric_cast<unsigned int>(1LL<<31);
	assert(ErrorTrap::is_good_and_reset());

	numeric_cast<unsigned int>(1LL<<32);
	assert(!ErrorTrap::is_good_and_reset());

	// floating point

	numeric_cast<signed char>(-128.0);
	assert(ErrorTrap::is_good_and_reset());

	numeric_cast<signed char>(-128.1);
	assert(!ErrorTrap::is_good_and_reset());

	numeric_cast<signed char>(127.0);
	assert(ErrorTrap::is_good_and_reset());

	numeric_cast<signed char>(127.1);
	assert(!ErrorTrap::is_good_and_reset());
}

void test_earth_distance() {
	float earth_distance(const D2D1_POINT_2F& p1, const D2D1_POINT_2F& p2);

	float d;

	d = earth_distance(D2D1::Point2F(0, 0), D2D1::Point2F(0, 0));
	assert(abs(d - 0) < 100);

	d = earth_distance(D2D1::Point2F(0, 90), D2D1::Point2F(0, 0));
	assert(abs(d - 10*1000*1000) < 10000);

	d = earth_distance(D2D1::Point2F(0, 0), D2D1::Point2F(0, 90));
	assert(abs(d - 10*1000*1000) < 10000);

	d = earth_distance(D2D1::Point2F(0, -90), D2D1::Point2F(0, 90));
	assert(abs(d - 20*1000*1000) < 20000);

	d = earth_distance(D2D1::Point2F(0, 0), D2D1::Point2F(180, 0));
	assert(abs(d - 20*1000*1000) < 20000);

	d = earth_distance(D2D1::Point2F(0, 0), D2D1::Point2F(-180, 0));
	assert(abs(d - 20*1000*1000) < 20000);
}

void tests() {
	#ifdef _DEBUG
	TRACE();
	ErrorTrap::set_stay_alive(true);

	test_floating_point_exceptions();
	test_numeric_cast();

	ErrorTrap::set_stay_alive(false);
	TRACE();
	#endif
}
