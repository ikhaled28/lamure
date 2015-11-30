// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include <lamure/ren/bvh_stream.h>

namespace lamure {
namespace ren {

bvh_stream::
bvh_stream()
: filename_(""),
  num_segments_(0) {


}

bvh_stream::
~bvh_stream() {
    closeStream(false);
}

void bvh_stream::
openStream(const std::string& bvh_filename,
           const bvh_stream_type type) {

    closeStream(false);
    
    num_segments_ = 0;
    filename_ = bvh_filename;
    type_ = type;

    std::ios::openmode mode = std::ios::binary;

    if (type_ == bvh_stream_type::BVH_STREAM_IN) {
        mode |= std::ios::in;
    }
    if (type_ == bvh_stream_type::BVH_STREAM_OUT) {
        mode |= std::ios::out;
        mode |= std::ios::trunc;
    }

    file_.open(filename_, mode);

    if (!file_.is_open()) {
        throw std::runtime_error(
            "PLOD: bvh_stream::Unable to open stream: " + filename_);
    }
   
}

void bvh_stream::
closeStream(const bool remove_file) {
    
    if (file_.is_open()) {
        if (type_ == bvh_stream_type::BVH_STREAM_OUT) {
            file_.flush();
        }
        file_.close();
        if (file_.fail()) {
            throw std::runtime_error(
                "PLOD: bvh_stream::Unable to close stream: " + filename_);
        }

        if (type_ == bvh_stream_type::BVH_STREAM_OUT) {
            if (remove_file) {
                if (std::remove(filename_.c_str())) {
                    throw std::runtime_error(
                        "PLOD: bvh_stream::Unable to delete file: " + filename_);
                }

            }
        }
    }

}


void bvh_stream::
write(bvh_stream::bvh_serializable& serializable) {

    if (!file_.is_open()) {
        throw std::runtime_error(
            "PLOD: bvh_stream::Unable to serialize: " + filename_);
    }

    bvh_sig sig;

    size_t allocated_size = sig.size() + serializable.size();
    size_t used_size = allocated_size;
    size_t padding = 0;
    while (allocated_size % 32 != 0) {
        ++allocated_size;
        ++padding;
    }
    
    serializable.signature(sig.signature_);

    sig.reserved_ = 0;
    sig.allocated_size_ = allocated_size - sig.size();
    sig.used_size_ = used_size - sig.size();

    sig.serialize(file_);
    serializable.serialize(file_);

    while (padding) {
        char c = 0;
        file_.write(&c, 1);
        --padding;
    }

}


void bvh_stream::
readbvh(const std::string& filename, bvh& bvh) {
 
    openStream(filename, bvh_stream_type::BVH_STREAM_IN);

    if (type_ != BVH_STREAM_IN) {
        throw std::runtime_error(
            "PLOD: bvh_stream::Failed to read bvh from: " + filename_);
    }
    if (!file_.is_open()) {
         throw std::runtime_error(
            "PLOD: bvh_stream::Failed to read bvh from: " + filename_);
    }
   
    //scan stream
    file_.seekg(0, std::ios::end);
    size_t filesize = (size_t)file_.tellg();
    file_.seekg(0, std::ios::beg);

    num_segments_ = 0;

    bvh_tree_seg tree;
    bvh_tree_extension_seg tree_ext;
    std::vector<bvh_node_seg> nodes;
    std::vector<bvh_node_extension_seg> nodes_ext;
    uint32_t tree_id = 0;
    uint32_t tree_ext_id = 0;
    uint32_t node_id = 0;
    uint32_t node_ext_id = 0;


    //go through entire stream and fetch the segments
    while (true) {
        bvh_sig sig;
        sig.deserialize(file_);
        if (sig.signature_[0] != 'B' ||
            sig.signature_[1] != 'V' ||
            sig.signature_[2] != 'H' ||
            sig.signature_[3] != 'X') {
             throw std::runtime_error(
                 "PLOD: bvh_stream::Invalid magic encountered: " + filename_);
        }
            
        size_t anchor = (size_t)file_.tellg();

        switch (sig.signature_[4]) {

            case 'F': { //"BVHXFILE"
                bvh_file_seg seg;
                seg.deserialize(file_);
                break;
            }
            case 'T': { 
                switch (sig.signature_[5]) {
                    case 'R': { //"BVHXTREE"
                        tree.deserialize(file_);
                        ++tree_id;
                        break;
                    }
                    case 'E': { //"BVHXTEXT"
                        tree_ext.deserialize(file_);
                        ++tree_ext_id;
                        break;                     
                    }
                    default: {
                        throw std::runtime_error(
                            "PLOD: bvh_stream::Stream corrupt -- Invalid segment encountered");
                        break;
                    }
                }
                break;
            }
            case 'N': { 
                switch (sig.signature_[5]) {
                    case 'O': { //"BVHXNODE"
                        bvh_node_seg node;
                        node.deserialize(file_);
                        nodes.push_back(node);
                        if (node_id != node.node_id_) {
                            throw std::runtime_error(
                                "PLOD: bvh_stream::Stream corrupt -- Invalid node order");
                        }
                        ++node_id;
                        break;
                    }
                    case 'E': { //"BVHXNEXT"
                        bvh_node_extension_seg node_ext;
                        node_ext.deserialize(file_);
                        nodes_ext.push_back(node_ext);
                        if (node_ext_id != node_ext.node_id_) {
                            throw std::runtime_error(
                                "PLOD: bvh_stream::Stream corrupt -- Invalid node extension order");
                        }
                        ++node_ext_id;
                        break;
                    }
                    default: {
                        throw std::runtime_error(
                            "PLOD: bvh_stream::Stream corrupt -- Invalid segment encountered");
                        break;
                    }
                }
                break;
            }
            default: {
                throw std::runtime_error(
                    "PLOD: bvh_stream::file corrupt -- Invalid segment encountered");
                break;
            }
        }

        if (anchor + sig.allocated_size_ < filesize) {
            file_.seekg(anchor + sig.allocated_size_, std::ios::beg);
        }
        else {
            break;
        }

    }

    closeStream(false);

    if (tree_id != 1) {
       throw std::runtime_error(
           "PLOD: bvh_stream::Stream corrupt -- Invalid number of bvh segments");
    }   

    if (tree_ext_id > 1) {
       throw std::runtime_error(
           "PLOD: bvh_stream::Stream corrupt -- Invalid number of bvh extensions");
    }    

    //Note: this is the rendering library version of the file reader!

    bvh.set_depth(tree.depth_);
    bvh.set_num_nodes(tree.num_nodes_);
    bvh.set_fan_factor(tree.fan_factor_);
    bvh.set_surfels_per_node(tree.max_surfels_per_node_);
    bvh.set_size_of_surfel(tree.serialized_surfel_size_);
    scm::math::vec3f translation(tree.translation_.x_,
                                tree.translation_.y_,
                                tree.translation_.z_);
    bvh.set_translation(translation);

    if (bvh.num_nodes() != node_id) {
       throw std::runtime_error(
           "PLOD: bvh_stream::Stream corrupt -- Ivalid number of node segments");
    }

    for (const auto& node : nodes) {
       scm::math::vec3f centroid(node.centroid_.x_,
                                 node.centroid_.y_,
                                 node.centroid_.z_);
       bvh.SetCentroid(node.node_id_, centroid);
       bvh.SetAvgsurfelRadius(node.node_id_, node.avg_surfel_radius_);
       bvh.SetVisibility(node.node_id_, (bvh::node_visibility)node.visibility_);
       scm::math::vec3f box_min(node.bounding_box_.min_.x_,
                                node.bounding_box_.min_.y_,
                                node.bounding_box_.min_.z_);
       scm::math::vec3f box_max(node.bounding_box_.max_.x_,
                                node.bounding_box_.max_.y_,
                                node.bounding_box_.max_.z_);
       bvh.Setbounding_box(node.node_id_, scm::gl::boxf(box_min, box_max));
   
    }

}

void bvh_stream::
writebvh(const std::string& filename, bvh& bvh) {

   openStream(filename, bvh_stream_type::BVH_STREAM_OUT);

   if (type_ != BVH_STREAM_OUT) {
       throw std::runtime_error(
           "PLOD: bvh_stream::Failed to append tree to: " + filename_);
   }
   if (!file_.is_open()) {
       throw std::runtime_error(
           "PLOD: bvh_stream::Failed to append tree to: " + filename_);
   }
   
   file_.seekp(0, std::ios::beg);

   bvh_file_seg seg;
   seg.major_version_ = 0;
   seg.minor_version_ = 1;
   seg.reserved_ = 0;

   write(seg);

   //Note: This is the rendering library version of the file writer!
   
   bvh_tree_seg tree;
   tree.segment_id_ = num_segments_++;
   tree.depth_ = bvh.depth();
   tree.num_nodes_ = bvh.num_nodes();
   tree.fan_factor_ = bvh.fan_factor();
   tree.max_surfels_per_node_ = bvh.surfels_per_node();
   tree.serialized_surfel_size_ = bvh.size_of_surfel();
   tree.reserved_0_ = 0;
   tree.state_ = bvh_tree_state::BVH_STATE_SERIALIZED;
   tree.reserved_1_ = 0;
   tree.reserved_2_ = 0;
   tree.translation_.x_ = bvh.translation().x;
   tree.translation_.y_ = bvh.translation().y;
   tree.translation_.z_ = bvh.translation().z;
   tree.reserved_3_ = 0;

   write(tree);

   for (uint32_t node_id = 0; node_id < bvh.num_nodes(); ++node_id) {
       bvh_node_seg node;
       node.segment_id_ = num_segments_++;
       node.node_id_ = node_id;
       const scm::math::vec3f centroid = bvh.GetCentroid(node_id);
       node.centroid_.x_ = centroid.x;
       node.centroid_.y_ = centroid.y;
       node.centroid_.z_ = centroid.z;
       node.depth_ = bvh.GetDepthOfNode(node_id);
       node.reduction_error_ = 0.f;
       node.avg_surfel_radius_ = bvh.GetAvgsurfelRadius(node_id);
       node.visibility_ = (bvh_node_visibility)bvh.GetVisibility(node_id);
       node.reserved_ = 0;
       const scm::gl::boxf box = bvh.Getbounding_box(node_id);
       node.bounding_box_.min_.x_ = box.min_vertex().x;
       node.bounding_box_.min_.y_ = box.min_vertex().y;
       node.bounding_box_.min_.z_ = box.min_vertex().z;
       node.bounding_box_.max_.x_ = box.max_vertex().x;
       node.bounding_box_.max_.y_ = box.max_vertex().y;
       node.bounding_box_.max_.z_ = box.max_vertex().z;

       write(node);
   }

   closeStream(false);

}



} } // namespace lamure

