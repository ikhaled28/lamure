// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#ifndef PRE_IO_PLY_PLY_PARSER_H_
#define PRE_IO_PLY_PLY_PARSER_H_

#include <fstream>
#include <iostream>
#include <istream>
#include <sstream>
#include <string>
#include <vector>

#include <functional>
#include <memory>

#include <boost/mpl/fold.hpp>
#include <boost/mpl/inherit.hpp>
#include <boost/mpl/inherit_linearly.hpp>
#include <boost/mpl/joint_view.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>

#include <lamure/pre/io/ply/ply.h>
#include <lamure/pre/io/ply/byte_order.h>
#include <lamure/pre/io/ply/io_operators.h>

namespace lamure
{
namespace pre
{
namespace io
{
namespace ply
{

class ply_parser
{
public:

    typedef std::function<void(std::size_t, const std::string &)> info_callback_type;
    typedef std::function<void(std::size_t, const std::string &)> warning_callback_type;
    typedef std::function<void(std::size_t, const std::string &)> error_callback_type;

    typedef std::function<void()> magic_callback_type;
    typedef std::function<void(format_type, const std::string &)> format_callback_type;
    typedef std::function<void(const std::string &)> comment_callback_type;
    typedef std::function<void(const std::string &)> obj_info_callback_type;
    typedef std::function<bool()> end_header_callback_type;

    typedef std::function<void()> begin_element_callback_type;
    typedef std::function<void()> end_element_callback_type;
    typedef std::tuple<begin_element_callback_type, end_element_callback_type> element_callbacks_type;
    typedef std::function<element_callbacks_type(const std::string &, std::size_t)> element_definition_callback_type;

    template<typename ScalarType>
    struct scalar_property_callback_type
    {
        typedef std::function<void(ScalarType)> type;
    };

    template<typename ScalarType>
    struct scalar_property_definition_callback_type
    {
        typedef typename scalar_property_callback_type<ScalarType>::type scalar_property_callback_type;
        typedef std::function<scalar_property_callback_type(const std::string &, const std::string &)> type;
    };

    typedef boost::mpl::vector<int8, int16, int32, uint8, uint16, uint32, float32, float64> scalar_types;

    class scalar_property_definition_callbacks_type
    {
    private:
        template<typename T>
        struct callbacks_element
        {
            typedef T scalar_type;
            typename scalar_property_definition_callback_type<scalar_type>::type callback;
        };
        typedef boost::mpl::inherit_linearly<
            scalar_types,
            boost::mpl::inherit<
                boost::mpl::_1,
                callbacks_element<boost::mpl::_2>
            >
        >::type callbacks;
        callbacks callbacks_;
    public:
        template<typename ScalarType>
        const typename scalar_property_definition_callback_type<ScalarType>::type &get() const
        {
            return static_cast<const callbacks_element<ScalarType> &>(callbacks_).callback;
        }
        template<typename ScalarType>
        typename scalar_property_definition_callback_type<ScalarType>::type &get()
        {
            return static_cast<callbacks_element<ScalarType> &>(callbacks_).callback;
        }
        template<typename ScalarType>
        inline friend typename scalar_property_definition_callback_type<ScalarType>::type &at(scalar_property_definition_callbacks_type &scalar_property_definition_callbacks)
        {
            return scalar_property_definition_callbacks.get<ScalarType>();
        }
        template<typename ScalarType>
        inline friend const typename scalar_property_definition_callback_type<ScalarType>::type &at(const scalar_property_definition_callbacks_type &scalar_property_definition_callbacks)
        {
            return scalar_property_definition_callbacks.get<ScalarType>();
        }
    };

    template<typename ScalarType>
    typename scalar_property_definition_callback_type<ScalarType>::type &at(scalar_property_definition_callbacks_type &scalar_property_definition_callbacks);

    template<typename SizeType, typename ScalarType>
    struct list_property_begin_callback_type
    {
        typedef std::function<void(SizeType)> type;
    };

    template<typename SizeType, typename ScalarType>
    struct list_property_element_callback_type
    {
        typedef std::function<void(ScalarType)> type;
    };

    template<typename SizeType, typename ScalarType>
    struct list_property_end_callback_type
    {
        typedef std::function<void()> type;
    };

    template<typename SizeType, typename ScalarType>
    struct list_property_definition_callback_type
    {
        typedef typename list_property_begin_callback_type<SizeType, ScalarType>::type list_property_begin_callback_type;
        typedef typename list_property_element_callback_type<SizeType, ScalarType>::type list_property_element_callback_type;
        typedef typename list_property_end_callback_type<SizeType, ScalarType>::type list_property_end_callback_type;
        typedef std::function<
            std::tuple<
                list_property_begin_callback_type,
                list_property_element_callback_type,
                list_property_end_callback_type
            >(const std::string &, const std::string &)> type;
    };

    typedef boost::mpl::vector<uint8, uint16, uint32> size_types;

    class list_property_definition_callbacks_type
    {
    private:
        template<typename T>
        struct pair_with: boost::mpl::pair<T, boost::mpl::_>
        {
        };
        template<typename Sequence1, typename Sequence2>
        struct sequence_product:
            boost::mpl::fold<
                Sequence1,
                boost::mpl::vector0<>,
                boost::mpl::joint_view<
                    boost::mpl::_1,
                    boost::mpl::transform<
                        Sequence2,
                        pair_with<boost::mpl::_2>
                    >
                >
            >
        {
        };
        template<typename T>
        struct callbacks_element
        {
            typedef typename T::first size_type;
            typedef typename T::second scalar_type;
            typename list_property_definition_callback_type<size_type, scalar_type>::type callback;
        };
        typedef boost::mpl::inherit_linearly<
            sequence_product<size_types, scalar_types>::type,
            boost::mpl::inherit<
                boost::mpl::_1,
                callbacks_element<boost::mpl::_2>
            >
        >::type callbacks;
        callbacks callbacks_;
    public:
        template<typename SizeType, typename ScalarType>
        typename list_property_definition_callback_type<SizeType, ScalarType>::type &get()
        {
            return static_cast<callbacks_element<boost::mpl::pair<SizeType, ScalarType> > &>(callbacks_).callback;
        }
        template<typename SizeType, typename ScalarType>
        const typename list_property_definition_callback_type<SizeType, ScalarType>::type &get() const
        {
            return static_cast<const callbacks_element<boost::mpl::pair<SizeType, ScalarType> > &>(callbacks_).callback;
        }
        template<typename SizeType, typename ScalarType>
        friend typename list_property_definition_callback_type<SizeType, ScalarType>::type &at(list_property_definition_callbacks_type &list_property_definition_callbacks)
        {
            return list_property_definition_callbacks.get<SizeType, ScalarType>();
        }
        template<typename SizeType, typename ScalarType>
        friend const typename list_property_definition_callback_type<SizeType, ScalarType>::type &at(const list_property_definition_callbacks_type &list_property_definition_callbacks)
        {
            return list_property_definition_callbacks.get<SizeType, ScalarType>();
        }
    };

    void info_callback(const info_callback_type &info_callback);
    void warning_callback(const warning_callback_type &warning_callback);
    void error_callback(const error_callback_type &error_callback);
    void magic_callback(const magic_callback_type &magic_callback);
    void format_callback(const format_callback_type &format_callback);
    void element_definition_callback(const element_definition_callback_type &element_definition_callback);
    void scalar_property_definition_callbacks(const scalar_property_definition_callbacks_type &scalar_property_definition_callbacks);
    void list_property_definition_callbacks(const list_property_definition_callbacks_type &list_property_definition_callbacks);
    void comment_callback(const comment_callback_type &comment_callback);
    void obj_info_callback(const obj_info_callback_type &obj_info_callback);
    void end_header_callback(const end_header_callback_type &end_header_callback);

    typedef int flags_type;
    enum flags
    {
    };

    ply_parser(flags_type flags = 0);
    bool parse(std::istream &istream);
    bool parse(const std::string &filename);

private:

    struct property
    {
        property(const std::string &name)
            : name(name)
        {}
        virtual ~property()
        {}
        virtual bool parse(class ply_parser &ply_parser, format_type format, std::istream &istream) = 0;
        std::string name;
    };

    template<typename ScalarType>
    struct scalar_property: public property
    {
        typedef ScalarType scalar_type;
        typedef typename scalar_property_callback_type<scalar_type>::type callback_type;
        scalar_property(const std::string &name, callback_type callback)
            : property(name), callback(callback)
        {}
        bool parse(class ply_parser &ply_parser, format_type format, std::istream &istream)
        { return ply_parser.parse_scalar_property<scalar_type>(format, istream, callback); }
        callback_type callback;
    };

    template<typename SizeType, typename ScalarType>
    struct list_property: public property
    {
        typedef SizeType size_type;
        typedef ScalarType scalar_type;
        typedef typename list_property_begin_callback_type<size_type, scalar_type>::type begin_callback_type;
        typedef typename list_property_element_callback_type<size_type, scalar_type>::type element_callback_type;
        typedef typename list_property_end_callback_type<size_type, scalar_type>::type end_callback_type;
        list_property(const std::string &name, begin_callback_type begin_callback, element_callback_type element_callback, end_callback_type end_callback)
            : property(name), begin_callback(begin_callback), element_callback(element_callback), end_callback(end_callback)
        {}
        bool parse(class ply_parser &ply_parser, format_type format, std::istream &istream)
        { return ply_parser.parse_list_property<size_type, scalar_type>(format, istream, begin_callback, element_callback, end_callback); }
        begin_callback_type begin_callback;
        element_callback_type element_callback;
        end_callback_type end_callback;
    };

    struct element
    {
        element(const std::string &name, std::size_t count, const begin_element_callback_type &begin_element_callback, const end_element_callback_type &end_element_callback)
            : name(name), count(count), begin_element_callback(begin_element_callback), end_element_callback(end_element_callback)
        {}
        std::string name;
        std::size_t count;
        begin_element_callback_type begin_element_callback;
        end_element_callback_type end_element_callback;
        std::vector<std::shared_ptr<property> > properties;
    };

    flags_type flags_;

    info_callback_type info_callback_;
    warning_callback_type warning_callback_;
    error_callback_type error_callback_;

    magic_callback_type magic_callback_;
    format_callback_type format_callback_;
    element_definition_callback_type element_definition_callbacks_;
    scalar_property_definition_callbacks_type scalar_property_definition_callbacks_;
    list_property_definition_callbacks_type list_property_definition_callbacks_;
    comment_callback_type comment_callback_;
    obj_info_callback_type obj_info_callback_;
    end_header_callback_type end_header_callback_;

    template<typename ScalarType>
    void parse_scalar_property_definition(const std::string &property_name);
    template<typename SizeType, typename ScalarType>
    void parse_list_property_definition(const std::string &property_name);

    template<typename ScalarType>
    bool parse_scalar_property(format_type format, std::istream &istream, const typename scalar_property_callback_type<ScalarType>::type &scalar_property_callback);
    template<typename SizeType, typename ScalarType>
    bool parse_list_property(format_type format,
                             std::istream &istream,
                             const typename list_property_begin_callback_type<SizeType, ScalarType>::type &list_property_begin_callback,
                             const typename list_property_element_callback_type<SizeType, ScalarType>::type &list_property_element_callback,
                             const typename list_property_end_callback_type<SizeType, ScalarType>::type &list_property_end_callback);

    std::size_t line_number_;
    element *current_element_;
};

inline ply_parser::ply_parser(flags_type flags)
    : flags_(flags)
{}

inline bool ply_parser::parse(const std::string &filename)
{
    std::ifstream ifstream(filename.c_str());
    return parse(ifstream);
}

inline void ply_parser::info_callback(const info_callback_type &info_callback)
{
    info_callback_ = info_callback;
}

inline void ply_parser::warning_callback(const warning_callback_type &warning_callback)
{
    warning_callback_ = warning_callback;
}

inline void ply_parser::error_callback(const error_callback_type &error_callback)
{
    error_callback_ = error_callback;
}

inline void ply_parser::magic_callback(const magic_callback_type &magic_callback)
{
    magic_callback_ = magic_callback;
}

inline void ply_parser::format_callback(const format_callback_type &format_callback)
{
    format_callback_ = format_callback;
}

inline void ply_parser::element_definition_callback(const element_definition_callback_type &element_definition_callback)
{
    element_definition_callbacks_ = element_definition_callback;
}

inline void ply_parser::scalar_property_definition_callbacks(const scalar_property_definition_callbacks_type &scalar_property_definition_callbacks)
{
    scalar_property_definition_callbacks_ = scalar_property_definition_callbacks;
}

inline void ply_parser::list_property_definition_callbacks(const list_property_definition_callbacks_type &list_property_definition_callbacks)
{
    list_property_definition_callbacks_ = list_property_definition_callbacks;
}

inline void ply_parser::comment_callback(const comment_callback_type &comment_callback)
{
    comment_callback_ = comment_callback;
}

inline void ply_parser::obj_info_callback(const obj_info_callback_type &obj_info_callback)
{
    obj_info_callback_ = obj_info_callback;
}

inline void ply_parser::end_header_callback(const end_header_callback_type &end_header_callback)
{
    end_header_callback_ = end_header_callback;
}

template<typename ScalarType>
inline void ply_parser::parse_scalar_property_definition(const std::string &property_name)
{
    typedef ScalarType scalar_type;
    typename scalar_property_definition_callback_type<scalar_type>::type &scalar_property_definition_callback = scalar_property_definition_callbacks_.get<scalar_type>();
    typename scalar_property_callback_type<scalar_type>::type scalar_property_callback;
    if (scalar_property_definition_callback) {
        scalar_property_callback = scalar_property_definition_callback(current_element_->name, property_name);
    }
    if (!scalar_property_callback) {
        if (warning_callback_) {
            warning_callback_(line_number_, "property �" + std::string(type_traits<scalar_type>::name()) + " " + property_name + "� of element �" + current_element_->name + "� is not handled");
        }
    }
    current_element_->properties.push_back(std::shared_ptr<property>(new scalar_property<scalar_type>(property_name, scalar_property_callback)));
}

template<typename SizeType, typename ScalarType>
inline void ply_parser::parse_list_property_definition(const std::string &property_name)
{
    typedef SizeType size_type;
    typedef ScalarType scalar_type;
    typename list_property_definition_callback_type<size_type, scalar_type>::type &list_property_definition_callback = list_property_definition_callbacks_.get<size_type, scalar_type>();
    typedef typename list_property_begin_callback_type<size_type, scalar_type>::type list_property_begin_callback_type;
    typedef typename list_property_element_callback_type<size_type, scalar_type>::type list_property_element_callback_type;
    typedef typename list_property_end_callback_type<size_type, scalar_type>::type list_property_end_callback_type;
    std::tuple<list_property_begin_callback_type, list_property_element_callback_type, list_property_end_callback_type> list_property_callbacks;
    if (list_property_definition_callback) {
        list_property_callbacks = list_property_definition_callback(current_element_->name, property_name);
    }
    if (!std::get<0>(list_property_callbacks) || !std::get<1>(list_property_callbacks) || !std::get<2>(list_property_callbacks)) {
        if (warning_callback_) {
            warning_callback_(line_number_,
                              "property �list " + std::string(type_traits<size_type>::name()) + " " + std::string(type_traits<scalar_type>::name()) + " " + property_name + "� of element �"
                                  + current_element_->name + "� is not handled");
        }
    }
    current_element_->properties.push_back(std::shared_ptr<property>(new list_property<size_type, scalar_type>(property_name,
                                                                                                               std::get<0>(list_property_callbacks),
                                                                                                               std::get<1>(list_property_callbacks),
                                                                                                               std::get<2>(list_property_callbacks))));
}

template<typename ScalarType>
inline bool ply_parser::parse_scalar_property(format_type format, std::istream &istream, const typename scalar_property_callback_type<ScalarType>::type &scalar_property_callback)
{
    std::locale loc;

    using namespace io_operators;
    typedef ScalarType scalar_type;
    if (format == ascii_format) {
        scalar_type value = 0;
        char space = ' ';
        istream >> value;
        if (!istream.eof()) {
            istream >> space >> std::ws;
        }
        if (!istream || !std::isspace(space, loc)) {
            if (error_callback_) {
                error_callback_(line_number_, "parse error");
            }
            return false;
        }
        if (scalar_property_callback) {
            scalar_property_callback(value);
        }
        return true;
    }
    else {
        scalar_type value = 0;
        istream.read(reinterpret_cast<char *>(&value), sizeof(scalar_type));
        if (!istream) {
            if (error_callback_) {
                error_callback_(line_number_, "parse error");
            }
            return false;
        }
        if (((format == binary_big_endian_format) && (host_byte_order == little_endian_byte_order)) || ((format == binary_little_endian_format) && (host_byte_order == big_endian_byte_order))) {
            swap_byte_order(value);
        }
        if (scalar_property_callback) {
            scalar_property_callback(value);
        }
        return true;
    }
}

template<typename SizeType, typename ScalarType>
inline bool ply_parser::parse_list_property(format_type format,
                                            std::istream &istream,
                                            const typename list_property_begin_callback_type<SizeType, ScalarType>::type &list_property_begin_callback,
                                            const typename list_property_element_callback_type<SizeType, ScalarType>::type &list_property_element_callback,
                                            const typename list_property_end_callback_type<SizeType, ScalarType>::type &list_property_end_callback)
{
    std::locale loc;

    using namespace io_operators;
    typedef SizeType size_type;
    typedef ScalarType scalar_type;
    if (format == ascii_format) {
        size_type size;
        char space = ' ';
        istream >> size;
        if (!istream.eof()) {
            istream >> space >> std::ws;
        }
        if (!istream || !std::isspace(space, loc)) {
            if (error_callback_) {
                error_callback_(line_number_, "parse error");
            }
            return false;
        }
        if (list_property_begin_callback) {
            list_property_begin_callback(size);
        }
        for (std::size_t index = 0; index < size; ++index) {
            scalar_type value;
            char space = ' ';
            istream >> value;
            if (!istream.eof()) {
                istream >> space >> std::ws;
            }
            if (!istream || !std::isspace(space, loc)) {
                if (error_callback_) {
                    error_callback_(line_number_, "parse error");
                }
                return false;
            }
            if (list_property_element_callback) {
                list_property_element_callback(value);
            }
        }
        if (list_property_end_callback) {
            list_property_end_callback();
        }
        return true;
    }
    else {
        size_type size;
        istream.read(reinterpret_cast<char *>(&size), sizeof(size_type));
        if (((format == binary_big_endian_format) && (host_byte_order == little_endian_byte_order)) || ((format == binary_little_endian_format) && (host_byte_order == big_endian_byte_order))) {
            swap_byte_order(size);
        }
        if (!istream) {
            if (error_callback_) {
                error_callback_(line_number_, "parse error");
            }
            return false;
        }
        if (list_property_begin_callback) {
            list_property_begin_callback(size);
        }
        for (std::size_t index = 0; index < size; ++index) {
            scalar_type value;
            istream.read(reinterpret_cast<char *>(&value), sizeof(scalar_type));
            if (!istream) {
                if (error_callback_) {
                    error_callback_(line_number_, "parse error");
                }
                return false;
            }
            if (((format == binary_big_endian_format) && (host_byte_order == little_endian_byte_order)) || ((format == binary_little_endian_format) && (host_byte_order == big_endian_byte_order))) {
                swap_byte_order(value);
            }
            if (list_property_element_callback) {
                list_property_element_callback(value);
            }
        }
        if (list_property_end_callback) {
            list_property_end_callback();
        }
        return true;
    }
}

}
}
}
} // namespace lamure::pre::io::ply

#endif // PRE_IO_PLY_PLY_PARSER_H_
