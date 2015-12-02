// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include <lamure/ren/ooc_pool.h>

namespace lamure
{

namespace ren
{

ooc_pool::
ooc_pool(const uint32_t num_threads, const size_t size_of_slot_in_bytes)
: locked_(false),
  size_of_slot_(size_of_slot_in_bytes),
  num_threads_(num_threads),
  shutdown_(false),
  bytes_loaded_(0) {
    assert(num_threads_ > 0);

    //configure semaphore
    semaphore_.set_min_signal_count(1);
    semaphore_.set_max_signal_count(std::numeric_limits<size_t>::max());

    model_database* database = model_database::get_instance();

    priority_queue_.initialize(LAMURE_CUT_UPDATE_LOADING_QUEUE_MODE, database->num_models());

    for (uint32_t i = 0; i < num_threads_; ++i) {
        threads_.push_back(std::thread(&ooc_pool::run, this));
    }
}

ooc_pool::
~ooc_pool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        shutdown_ = true;
        semaphore_.shutdown();
    }

    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();

}

bool ooc_pool::
is_shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    return shutdown_;
}

void ooc_pool::
lock() {
    mutex_.lock();
    locked_ = true;
}

void ooc_pool::
unlock() {
    locked_ = false;
    mutex_.unlock();
}

void ooc_pool::
begin_measure() {
  std::lock_guard<std::mutex> lock(mutex_);
  bytes_loaded_ = 0;
}

void ooc_pool::
end_measure() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "megabytes loaded: " << bytes_loaded_ / 1024 / 1024 << std::endl;
}

void ooc_pool::
run() {
    model_database* database = model_database::get_instance();
    model_t num_models = database->num_models();

    std::vector<LodStream*> lod_streams;

    for (model_t model_id = 0; model_id < num_models; ++model_id) {
        std::string bvh_filename = database->get_model(model_id)->get_bvh()->filename();
        std::string lod_file_name = bvh_filename.substr(0, bvh_filename.size()-3) + "lod";

        LodStream* access = new LodStream();
        access->open(lod_file_name);
        lod_streams.push_back(access);
    }

    char* local_cache = new char[size_of_slot_];

    while (true) {
        semaphore_.wait();

        if (is_shutdown())
            break;

        cache_queue::job job = priority_queue_.Topjob();

        if (job.node_id_ != invalid_node_t) {
            assert(job.slot_mem_ != nullptr);

            size_t stride_in_bytes = database->size_of_surfel() * 
               database->get_model(job.model_id_)->get_bvh()->surfels_per_node();
            size_t offset_in_bytes = job.node_id_ * stride_in_bytes;

            lod_streams[job.model_id_]->read(local_cache, offset_in_bytes, stride_in_bytes);

            {
                std::lock_guard<std::mutex> lock(mutex_);
                bytes_loaded_ += stride_in_bytes;
                memcpy(job.slot_mem_, local_cache, stride_in_bytes);
                history_.push_back(job);
            }

        }

    }

    {
        for (model_t model_id = 0; model_id < num_models; ++model_id) {
            LodStream* lod_stream = lod_streams[model_id];
            if (lod_stream != nullptr) {
                lod_stream->close();
                delete lod_stream;
                lod_stream = nullptr;
            }
        }
        lod_streams.clear();

        if (local_cache != nullptr) {
            delete[] local_cache;
            local_cache = nullptr;
        }
    }
}

void ooc_pool::
resolve_cache_histogramory(cache_index* index) {
    assert(locked_);

    for (auto entry : history_) {
        index->applySlot(entry.slot_id_, entry.model_id_, entry.node_id_);
        priority_queue_.pop_job(entry);
    }

    history_.clear();

}

void ooc_pool::
perform_queue_maintenance(cache_index* index) {
    assert(locked_);

    size_t num_jobs = priority_queue_.Numjobs();
    for (size_t i = 0; i < num_jobs; ++i) {
        cache_queue::job job = priority_queue_.Topjob();

        assert(job.slot_id_ != invalid_slot_t);

        priority_queue_.pop_job(job);
        index->unreserve_slot(job.slot_id_);

    }
}

bool ooc_pool::
acknowledge_request(cache_queue::job job) {
    bool success = priority_queue_.push_job(job);

    if (success) {
        semaphore_.Signal(1);
    }

    return success;
}

cache_queue::query_result ooc_pool::
acknowledge_query(const model_t model_id, const node_t node_id) {
    return priority_queue_.IsNodeIndexed(model_id, node_id);
}

void ooc_pool::
acknowledge_update(const model_t model_id, const node_t node_id, int32_t priority) {
    priority_queue_.Updatejob(model_id, node_id, priority);
}



} // namespace ren

} // namespace lamure

