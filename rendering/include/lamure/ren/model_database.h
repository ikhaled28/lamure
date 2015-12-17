// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#ifndef REN_MODEL_DATABASE_H_
#define REN_MODEL_DATABASE_H_

#include <unordered_map>
#include <mutex>

#include <lamure/utils.h>
#include <lamure/types.h>
#include <lamure/ren/dataset.h>
#include <lamure/ren/config.h>
#include <lamure/ren/platform.h>

#include <scm/gl_core/query_objects.h>

namespace lamure {
namespace ren {

class RENDERING_DLL model_database
{
public:

                        model_database(const model_database&) = delete;
                        model_database& operator=(const model_database&) = delete;
    virtual             ~model_database();

    static model_database* get_instance();

    const model_t       add_model(const std::string& filepath, const std::string& model_key);
    dataset*    get_model(const model_t model_id);
    void                apply();

    const model_t       num_models() const { std::lock_guard<std::mutex> lock(mutex_); return num_datasets_; };
    const size_t        size_of_surfel() const { std::lock_guard<std::mutex> lock(mutex_); return size_of_surfel_; };
    const size_t        surfels_per_node() const { std::lock_guard<std::mutex> lock(mutex_); return surfels_per_node_; };

protected:

                        model_database();
    static bool         is_instanced_;
    static model_database* single_;

private:
    static std::mutex   mutex_;

    std::unordered_map<model_t, dataset*> datasets_;

    model_t             num_datasets_;
    model_t             num_datasets_pending_;
    size_t              surfels_per_node_;
    size_t              surfels_per_node_pending_;
    size_t              size_of_surfel_;

};


} } // namespace lamure


#endif // REN_MODEL_DATABASE_H_
