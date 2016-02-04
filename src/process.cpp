#include "shared.h"

#include "duplicate.h"
#include "image.h"
#include "job.h"
#include "window.h"

#include <algorithm>
#include <mutex>
#include <vector>

float earth_distance(const D2D1_POINT_2F& p1, const D2D1_POINT_2F& p2) {
	assert(p1.x >= -180 && p1.x <= 180);
	assert(p2.x >= -180 && p2.x <= 180);
	assert(p1.y >= -90 && p1.y <= 90);
	assert(p2.y >= -90 && p2.y <= 90);

	const auto earth_mean_radius = 6371*1000.0f;
	const auto pi = 3.14159265358979323846f;

	D2D1_POINT_2F p1r = D2D1::Point2F(p1.x * (pi / 180), p1.y * (pi / 180));
	D2D1_POINT_2F p2r = D2D1::Point2F(p2.x * (pi / 180), p2.y * (pi / 180));

	auto dy = p2r.y - p1r.y;
	auto dx = p2r.x - p1r.x;

	auto a =
		sin(dy / 2) * sin(dy / 2) +
		cos(p1r.y) * cos(p2r.y) *
		sin(dx / 2) * sin(dx / 2);
	auto c = 2 * atan2(sqrt(a), sqrt(1-a));
	auto d = earth_mean_radius * c;

	assert(d >= 0);
	assert(d <= earth_mean_radius * earth_mean_radius * pi);

	return d;
}

static void thread_worker(Job* const job) {
	TRACE();

	et = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	for (;;) {
		auto ip = job->get_next_pair();
		const auto image_1 = ip.first;
		const auto image_2 = ip.second;

		bool quit =
			image_1 == nullptr ||
			image_2 == nullptr ||
			job->force_thread_exit ||
			!ErrorTrap::is_good();
		if (quit)
			break;

		if (image_1 == image_2)
			continue;

		auto distance_combined = 0.0f;
		auto distance_combined_min = 0.0f;
		auto distance_combined_max = 0.0f;

		// TODO: Magic numbers related to image pair similarity scoring below; should be refactored once it has stabilized.

		// score time
		auto distance_time = std::numeric_limits<float>::max();
		auto n_images_with_metadata_times =
			!image_1->get_metadata_times().empty() +
			!image_2->get_metadata_times().empty();
		if (n_images_with_metadata_times == 1) {
			distance_combined += 1;
		} else if (n_images_with_metadata_times == 2) {
			auto duration_min = std::chrono::system_clock::duration::max();
			for (auto t1 : image_1->get_metadata_times())
				for (auto t2 : image_2->get_metadata_times()) {
					// TODO: C++17: std::chrono::min(duration_min, std::chrono::abs(t1 - t2))
					auto dc = std::abs((t1 - t2).count());
					if (dc < duration_min.count())
						duration_min = std::chrono::system_clock::duration{dc};
				}
			assert(duration_min != std::chrono::system_clock::duration::max());

			if (duration_min != std::chrono::system_clock::duration::max())
				distance_time = std::chrono::duration<float>(duration_min).count();

			if (duration_min < std::chrono::hours{2*24})
				distance_combined += -5 * (1 - std::chrono::duration<float>(duration_min).count() / (2*24*3600));
			else if (duration_min > std::chrono::hours{20*24})
				distance_combined += 5;
		}
		distance_combined_min += -5;
		distance_combined_max += 5;

		// score location
		auto distance_location = std::numeric_limits<float>::max();
		auto x1 = image_1->get_metadata_position().x;
		auto y1 = image_1->get_metadata_position().y;
		auto x2 = image_2->get_metadata_position().x;
		auto y2 = image_2->get_metadata_position().y;
		if (x1 != 0 && y1 != 0 && x2 != 0 && y2 != 0) {
			auto d = earth_distance(
				image_1->get_metadata_position(),
				image_2->get_metadata_position());
			distance_location = d;
			if (d < 10*1000)
				distance_combined += -5*pow(1 - d / (10*1000), 2);
			else if (d > 100*1000)
				distance_combined += 5;
		} else if (x1 == 0 && y1 == 0 && (x2 != 0 || y2 != 0)) {
			distance_combined += 1;
		} else if (x2 == 0 && y2 == 0 && (x1 != 0 || y1 != 0)) {
			distance_combined += 1;
		}
		distance_combined_min += -5;
		distance_combined_max += 5;

		// score make and model
		if (image_1->get_metadata_make_model() == image_2->get_metadata_make_model()) {
			if (image_1->get_metadata_make_model().empty())
				distance_combined += 0; // both empty
			else
				distance_combined += -2; // both set and equal
		} else {
			if (image_1->get_metadata_make_model().empty() || image_2->get_metadata_make_model().empty())
				distance_combined += 1; // only one set
			else
				distance_combined += 5; // both set but different
		}
		distance_combined_min += -2;
		distance_combined_max += 5;

		// score camera id
		if (image_1->get_metadata_camera_id() == image_2->get_metadata_camera_id()) {
			if (image_1->get_metadata_camera_id().empty())
				distance_combined += 0;
			else
				distance_combined += -2;
		} else {
			if (image_1->get_metadata_camera_id().empty() || image_2->get_metadata_camera_id().empty())
				distance_combined += 1;
			else
				distance_combined += 5;
		}
		distance_combined_min += -2;
		distance_combined_max += 5;

		// score image id
		if (image_1->get_metadata_image_id() == image_2->get_metadata_image_id()) {
			if (image_1->get_metadata_image_id().empty())
				distance_combined += 0;
			else
				distance_combined += -10;
		} else {
			if (image_1->get_metadata_image_id().empty() || image_2->get_metadata_image_id().empty())
				distance_combined += 2;
			else
				distance_combined += 10;
		}
		distance_combined_min += -10;
		distance_combined_max += 10;

		// score dimensions
		auto ar1 = static_cast<float>(image_1->get_image_size().width) / image_1->get_image_size().height;
		auto ar2 = static_cast<float>(image_2->get_image_size().width) / image_2->get_image_size().height;
		ar1 = std::max(ar1, 1/ar1);
		ar2 = std::max(ar2, 1/ar2);
		if (abs(ar1 - ar2) > 0.01f)
			distance_combined += 1;
		distance_combined_min += 0;
		distance_combined_max += 1;

		// normalize distance
		distance_combined = (distance_combined - distance_combined_min) /
			(distance_combined_max - distance_combined_min);
		distance_combined *= 0.5f;

		// score visual similarity
		const auto distance_visual_max = 0.27f;
		auto distance_visual = std::numeric_limits<float>::max();
		distance_visual = image_1->get_distance(*image_2, distance_visual_max);
		distance_combined += distance_visual;

		// check status late so that attempts to decode pixels/metadata have been made
		auto images_ok =
			image_1->get_status() == Image::Status::ok &&
			image_2->get_status() == Image::Status::ok;
		if (!images_ok)
			continue;

		std::lock_guard<std::mutex> lg{job->duplicates_mutex};

		if (distance_visual < distance_visual_max)
			job->duplicates_visual.push_back(Duplicate{image_1, image_2, distance_visual});
		if (distance_time < 2*24*3600)
			job->duplicates_time.push_back(Duplicate{image_1, image_2, distance_time});
		if (distance_location < 10*1000)
			job->duplicates_location.push_back(Duplicate{image_1, image_2, distance_location});
		if (distance_combined < 0.46f)
			job->duplicates_combined.push_back(Duplicate{image_1, image_2, distance_combined});
	}

	CoUninitialize();
	TRACE();
}

std::vector<std::vector<Duplicate>> process(Window& window, const std::vector<std::wstring>& paths) {
	// prepare job
	std::vector<std::vector<Duplicate>> duplicate_categories{4};
	Job job(
		paths,
		duplicate_categories[0],
		duplicate_categories[1],
		duplicate_categories[2],
		duplicate_categories[3]);

	debug_timer_reset();

	// create workers
	std::vector<std::thread> threads{std::thread::hardware_concurrency()};
	for (auto& t : threads)
		t = std::thread(thread_worker, &job); // 44 byte one-time memory leak: https://connect.microsoft.com/VisualStudio/feedback/details/757212/vs-2012-rc-std-thread-reports-memory-leak-even-on-stack

	// update progress bar until no more work or window requests that work be stopped
	while (!job.is_completed()) {
		if (window.get_event().type == Event::Type::quit) {
			job.force_thread_exit = true;
			break;
		}
		window.set_progressbar_progress(0, job.get_progress());
	}

	if (!window.quit_event_seen()) {
		window.set_progressbar_progress(0, 1.0f);
		window.has_event();
	}

	// wait for threads to exit
	for (auto& thread : threads)
		thread.join();

	// return nothing if work not complete
	if (window.quit_event_seen())
		return {};

	for (auto& d : duplicate_categories)
		sort(d.begin(), d.end());

	debug_log << L"process time: " << debug_timer() << std::endl;
	debug_log << L"comparisons (calculated): " << (paths.size()*paths.size() - paths.size())/2 << std::endl;

	window.reset();

	return duplicate_categories;
}
