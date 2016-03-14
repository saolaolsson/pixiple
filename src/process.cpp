#include "shared.h"

#include "image.h"
#include "image_pair.h"
#include "job.h"
#include "window.h"

#include "shared/vector.h"

#include <algorithm>
#include <mutex>
#include <vector>

static void thread_worker(Job* const job) {
	TRACE();

	er = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	for (;;) {
		auto ip = job->get_next_pair();
		const auto i1 = ip.image_1;
		const auto i2 = ip.image_2;

		bool quit =
			i1 == nullptr ||
			i2 == nullptr ||
			job->force_thread_exit ||
			!ErrorReflector::is_good();
		if (quit)
			break;

		if (i1 == i2)
			continue;

		auto images_ok =
			i1->get_status() == Image::Status::ok &&
			i2->get_status() == Image::Status::ok;
		if (!images_ok)
			continue;

		auto distance_combined = 0.0f;
		auto distance_combined_min = 0.0f;
		auto distance_combined_max = 0.0f;

		// TODO: Magic numbers related to image pair similarity scoring below; should be refactored once it has stabilized.

		// score time
		auto distance_time = std::numeric_limits<float>::max();
		auto n_images_with_metadata_times =
			!i1->get_metadata_times().empty() +
			!i2->get_metadata_times().empty();
		if (n_images_with_metadata_times == 1) {
			distance_combined += 1;
		} else if (n_images_with_metadata_times == 2) {
			auto duration_min = ip.time_distance();
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
		auto p1 = i1->get_metadata_position();
		auto p2 = i2->get_metadata_position();
		auto n_images_with_metadata_locations =
			(p1.x != 0 && p1.y != 0) +
			(p2.x != 0 && p2.y != 0);
		if (n_images_with_metadata_locations == 1) {
			distance_combined += 1;
		} else if (n_images_with_metadata_locations == 2) {
			auto d = ip.location_distance();
			distance_location = d;
			if (d < 10*1000)
				distance_combined += -5*pow(1 - d / (10*1000), 2);
			else if (d > 100*1000)
				distance_combined += 5;
		}
		distance_combined_min += -5;
		distance_combined_max += 5;

		// score make and model
		if (i1->get_metadata_make_model() == i2->get_metadata_make_model()) {
			if (i1->get_metadata_make_model().empty())
				distance_combined += 0; // both empty
			else
				distance_combined += -2; // both set and equal
		} else {
			if (i1->get_metadata_make_model().empty() || i2->get_metadata_make_model().empty())
				distance_combined += 1; // only one set
			else
				distance_combined += 5; // both set but different
		}
		distance_combined_min += -2;
		distance_combined_max += 5;

		// score camera id
		if (i1->get_metadata_camera_id() == i2->get_metadata_camera_id()) {
			if (i1->get_metadata_camera_id().empty())
				distance_combined += 0;
			else
				distance_combined += -2;
		} else {
			if (i1->get_metadata_camera_id().empty() || i2->get_metadata_camera_id().empty())
				distance_combined += 1;
			else
				distance_combined += 5;
		}
		distance_combined_min += -2;
		distance_combined_max += 5;

		// score image id
		if (i1->get_metadata_image_id() == i2->get_metadata_image_id()) {
			if (i1->get_metadata_image_id().empty())
				distance_combined += 0;
			else
				distance_combined += -10;
		} else {
			if (i1->get_metadata_image_id().empty() || i2->get_metadata_image_id().empty())
				distance_combined += 2;
			else
				distance_combined += 10;
		}
		distance_combined_min += -10;
		distance_combined_max += 10;

		// score dimensions
		auto ar1 = static_cast<float>(i1->get_image_size().w) / i1->get_image_size().h;
		auto ar2 = static_cast<float>(i2->get_image_size().w) / i2->get_image_size().h;
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
		distance_visual = i1->get_distance(*i2, distance_visual_max);
		distance_combined += distance_visual;

		std::lock_guard<std::mutex> lg{job->pairs_mutex};

		if (distance_visual < distance_visual_max) {
			ip.distance = distance_visual;
			job->pairs_visual.push_back(ip);
		}
		if (distance_time < 24*3600) {
			ip.distance = distance_time;
			job->pairs_time.push_back(ip);
		}
		if (distance_location < 10*1000) {
			ip.distance = distance_location;
			job->pairs_location.push_back(ip);
		}
		if (distance_combined < 0.46f) {
			ip.distance = distance_combined;
			job->pairs_combined.push_back(ip);
		}
	}

	CoUninitialize();
	TRACE();
}

std::vector<std::vector<ImagePair>> process(Window& window, const std::vector<std::tr2::sys::path>& paths) {
	// prepare job
	std::vector<std::vector<ImagePair>> pair_categories{4};
	Job job{
		paths,
		pair_categories[0],
		pair_categories[1],
		pair_categories[2],
		pair_categories[3]};

	debug_timer_reset();

	// create workers
	std::vector<std::thread> threads{std::thread::hardware_concurrency()};
	for (auto& t : threads)
		t = std::thread(thread_worker, &job);

	// update progress bar until no more work or window requests that work be stopped
	while (!job.is_completed()) {
		auto e = window.get_event();
		if (e.type == Event::Type::quit || e.type == Event::Type::button) {
			job.force_thread_exit = true;
			break;
		}
		window.set_progressbar_progress(0, job.get_progress());
	}

	// wait for threads to exit
	for (auto& thread : threads)
		thread.join();

	// return nothing if work not complete
	if (job.force_thread_exit)
		return {{}, {}, {}, {}};

	for (auto& d : pair_categories)
		sort(d.begin(), d.end());

	debug_log << L"process time: " << debug_timer() << std::endl;
	debug_log << L"comparisons (calculated): " << (paths.size()*paths.size() - paths.size())/2 << std::endl;

	return pair_categories;
}
