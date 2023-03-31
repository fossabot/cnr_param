/*
 *  Software License Agreement (New BSD License)
 *
 *  Copyright 2020 National Council of Research of Italy (CNR)
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CNR_PARAM_INCLUDE_CNR_PARAM_IMPL_YAML_CNR_PARAM_YAML_CPP_IMPL
#define CNR_PARAM_INCLUDE_CNR_PARAM_IMPL_YAML_CNR_PARAM_YAML_CPP_IMPL

#include <typeinfo>       // operator typeid
#include <typeindex>
#include <string>
#include <vector>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/type_index.hpp>

#include <cnr_param/cnr_param.hpp>

#include <cnr_param/utils/eigen.hpp>
#include <cnr_param/utils/string.hpp>
#include <cnr_param/utils/filesystem.hpp>
#include <cnr_param/utils/yaml.hpp>

#include <yaml-cpp/exceptions.h>

namespace cnr 
{
namespace param
{

// ==============================================================================
// ==============================================================================
inline bool absolutepath(const std::string& key, fs::path& ap, std::string& what)
{
  const char* env_p = std::getenv("CNR_PARAM_ROOT_DIRECTORY");
  if(!env_p)
  {
    what = (what.size() ? (what + "\n") : std::string("") ) +
      "The env variable CNR_PARAM_ROOT_DIRECTORY is not set!" ;
      return false;
  }
  std::string _key = key;
  while(_key.back()=='/')
  {
    _key.pop_back();
  }
  fs::path p = fs::path(std::string(env_p)) / (_key + ".yaml");

  std::string errmsg;
  if(!cnr::param::utils::checkfilepath(p.string(), errmsg))
  {
    what = (what.size() ? (what + "\n") : std::string("") ) +
      "The param '" + key + "' is not in param server.\n what(): " + errmsg;
    return false;
  }
  ap = fs::absolute(p);
  return true;
}

inline bool has(const std::string& key, std::string& what)
{
  fs::path ap; 
  if(!absolutepath(key, ap, what))
  {
    return false;
  }
  return true;
}

inline bool recover(const std::string& key, YAML::Node& node, std::string& what)
{
  fs::path ap; 
  if(!absolutepath(key, ap, what))
  {
    return false;
  }

  //Create a file mapping
  boost::interprocess::file_mapping file(ap.string().c_str(), boost::interprocess::read_only);

  //Map the whole file with read-write permissions in this process
  boost::interprocess::mapped_region region(file, boost::interprocess::read_only);

  //Get the address of the mapped region
  void * addr       = region.get_address();
  std::size_t size  = region.get_size();
  UNUSED(size);
  
  //Check that memory was initialized to 1
  const char *mem = static_cast<char*>(addr);
  std::string strmem(mem);

  auto config = YAML::Load(strmem);
  auto tokens = cnr::param::utils::tokenize(key, "/");
  node = config[tokens.back()];
  
  return bool(config[tokens.back()]);
}
// ==============================================================================
// ==============================================================================




// ==============================================================================
/**
 * @brief 
 * 
 * @tparam T 
 * @param key 
 * @param ret 
 * @param what 
 * @param default_val 
 * @return true 
 * @return false 
 */
// ==============================================================================
template<typename T>
inline bool get(const std::string& key, T& ret, std::string& what)
{
  if (!cnr::param::has(key, what))
  {
    return false;
  }

  YAML::Node node;
  if (!cnr::param::recover(key, node, what))
  {
    return false;
  }

  try
  {
    ret = cnr::param::get<T>(node);
  }
  catch (std::exception& e)
  {
    what = "Failed in getting the Node struct from parameter '" + key + "':\n";
    what += e.what();
    return false;
  }
  return true;
}

template<typename T>
inline bool get(const std::string& key, T& ret, std::string& what, const T& default_val)
{
  if (!cnr::param::has(key, what))
  {
    what = (what.size() ? (what + "\n") : std::string("") ) + "Try to superimpose default value...";
    if (!utils::resize(ret, default_val))
    {
      what += " Error!";
      return false;
    }
    what += " OK!";
    ret = default_val;
    return true;
  }

  YAML::Node node;
  if (!cnr::param::recover(key, node, what))
  {
    return false;
  }

  try
  {
    ret = cnr::param::get<T>(node);
  }
  catch (std::exception& e)
  {
    what = "Failed in getting the Node struct from parameter '" + key + "':\n";
    what += e.what();
    return false;
  }
  return true;
}

inline bool is_sequence(const std::string& key)
{
  YAML::Node node;
  std::string what;
  if (!cnr::param::recover(key, node, what))
  {
    return false;
  }
  return is_sequence(node);
}

inline bool is_sequence(const cnr::param::node_t& node)
{
  return node.IsSequence();
}

inline size_t size(const cnr::param::node_t& node)
{
  if(!is_sequence(node))
  {
    std::stringstream err;
    err << __PRETTY_FUNCTION__ << ":" << std::to_string(__LINE__)  << ": " 
      << "The node is not a sequence!";
    throw std::runtime_error(err.str().c_str());
  }
  return node.size();
}

inline bool get_leaf(const node_t& node, const std::string& key, node_t& leaf, std::string& what)
{
   if(node[key])
   {
    leaf = node[key];
    return true;
   }
   what = "The key is not in the node dictionary";
   return false;
}

template<typename T>
inline bool at(const cnr::param::node_t& node, std::size_t i, T& element, std::string& what)
{
  if(!is_sequence(node))
  {
    what = "The node is not a sequence";
    return false;
  }
  if(i>= node.size())
  {
    what = "The index is out the boundaries";
    return false;
  }

  if(node[i].IsScalar())
  {
    return get_scalar<T>(node[i], element);
  }
  
  what = std::string(__PRETTY_FUNCTION__ ) + ":" + std::to_string( __LINE__ ) + ": "
             "Error in the extraction of the element #" + std::to_string(i) + ". Type of the sequence: "
              + boost::typeindex::type_id_with_cvr<decltype(T())>().pretty_name() 
                + "' but the element is not a scalar type.";
  return false;
}

template<>
inline bool at(const cnr::param::node_t& node, std::size_t i, cnr::param::node_t& element, std::string& what)
{
  if(!is_sequence(node))
  {
    what = "It is not a sequence";
    return false;
  }
  if(i>= node.size())
  {
    what = "Index out of boundaries";
    return false;
  }

  element = node[i];

  return true;
}
// =============================================================================================



template<typename T>
inline T get(const YAML::Node& node)
{
  T ret;
  bool ok = false;
  std::stringstream what;
  if(node.IsScalar())
  {
    ok = get_scalar(node, ret, what);
  }
  else if(node.IsSequence())
  {
    ok = get_sequence<T>(node, ret, what);
  } 
  else if(node.IsMap())
  {
    ok = get_map<T>(node, ret, what);
  }
  else
  {
    what<< __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
          << "Tried to extract a ' "
            << boost::typeindex::type_id_with_cvr<decltype(T())>().pretty_name() 
              << "' but the type node is undefined." << std::endl
                << "Node: " << std::endl
                  << node << std::endl;
  }
  if(!ok)
  {
    throw std::runtime_error(what.str().c_str());
  }
  return ret;
}

template<>
inline YAML::Node get(const YAML::Node& node)
{
  YAML::Node ret(node);
  return ret;
}

#define CATCH(X)\
  catch(YAML::Exception& e)\
  {\
      std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "\
        << "YAML Exception, Error in the extraction of an object of type '"\
          << boost::typeindex::type_id_with_cvr<decltype( X )>().pretty_name() \
            << "'" << std::endl\
              << "Node: " << std::endl\
                << node << std::endl\
                  << "What: " << std::endl\
                    << e.what() << std::endl;\
    }\
    catch (std::exception& e)\
    {\
      std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "\
        << "Exception, Error in the extraction of an object of type '"\
          << boost::typeindex::type_id_with_cvr<decltype( X )>().pretty_name() \
            << "'" << std::endl\
              << "Node: " << std::endl\
                << node << std::endl\
                  << "What: " << std::endl\
                    << e.what() << std::endl;\
    }


// =============================================================================================
// SCALAR
// =============================================================================================
template<typename T>
inline bool _get_scalar(const YAML::Node& node, T& ret, std::stringstream& what)
{
  bool ok = false;
  YAML::Node config(node);
  if(!config.IsScalar())
  {
    what << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
      << "Tried to extract a ' "
        << boost::typeindex::type_id_with_cvr<decltype(T())>().pretty_name() 
          << "' but the node is not a scalar." << std::endl
            << "Node: " << std::endl
              << node << std::endl;
  }
  else
  {
    try
    {
      ret = node.as<T>();
      ok = true;
    }
    CATCH(ret);
  }
  return ok;
}

template<typename T>
inline bool get_scalar(const node_t& node, T& ret, std::stringstream& what)
{
  UNUSED(node);
  UNUSED(ret);
  what << "The type ' " << boost::typeindex::type_id_with_cvr<decltype(T())>().pretty_name() 
          << "' is not supported. You must specilized your own 'get_scalar' template function";
  return false;
}

template<>
inline bool get_scalar(const YAML::Node& node, double& ret, std::stringstream& what)
{
  return _get_scalar<double>(node, ret, what);
}

template<>
bool get_scalar(const YAML::Node& node, int& ret, std::stringstream& what)
{
  return _get_scalar<int>(node, ret, what);
}

template<>
bool get_scalar(const YAML::Node& node, bool& ret, std::stringstream& what)
{
  return _get_scalar<bool>(node, ret, what);
}

template<>
bool get_scalar(const YAML::Node& node, std::string& ret, std::stringstream& what)
{
  return _get_scalar<std::string>(node, ret, what);
}
// =============================================================================================
// END SCALAR
// =============================================================================================


// =============================================================================================
// SEQUENCE
// =============================================================================================
template<typename T>
inline bool get_sequence(const node_t& node, T& ret, std::stringstream& what)
{
  UNUSED(node);
  UNUSED(ret);
  what << "The type ' " <<
      boost::typeindex::type_id_with_cvr<decltype(T())>().pretty_name() 
        << "' is not supported. You must specilized your own 'get_sequence' template function";
  return false;
}

template<typename T>
inline bool get_sequence(const YAML::Node& node, std::vector<T>& ret, std::stringstream& what)
{
  bool ok = false;
  YAML::Node config(node);
  if(!config.IsSequence())
  {
    what << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": " 
      << "Tried to extract a ' "
        << boost::typeindex::type_id_with_cvr<decltype(T())>().pretty_name() 
          << "' but the node is not a sequence." << std::endl
            << "Node: " << std::endl
              << node << std::endl;
  }
  else
  {

    try
    {
      ret.clear();
      ret.resize(node.size());
      for(std::size_t i=0; i<node.size();i++)
      {
        T v = T();
        if(node[i].IsScalar())
        {
          ok = get_scalar<T>(node[i], v);
        }
        else if(node[i].IsSequence())
        {
          ok = get_sequence<T>(node[i], v);
        }
        else if(node[i].IsMap())
        {
          ok = get_map<T>(node[i], v);
        }

        if(!ok)
        {
          what  << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
                  << "Error in the extraction of the element #" << i << ". Type of the sequence: "
                    << boost::typeindex::type_id_with_cvr<decltype(std::vector<T>())>().pretty_name() 
                      << "' but the node is not a sequence." << std::endl
                        << "Node: " << std::endl
                          << node << std::endl;
          break;
        }
        ret.push_back(v);

      }
      ok = true;
    }
    CATCH(ret);
  }
  return ok;
}

template<typename T>
inline bool get_sequence(const YAML::Node& node, std::vector<std::vector<T>>& ret, std::stringstream& what)
{
  bool ok = false;
  YAML::Node config(node);
  if(!config.IsSequence())
  {
    what << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
      << "Tried to extract a ' "
        << boost::typeindex::type_id_with_cvr<decltype(T())>().pretty_name() 
          << "' but the node is not a sequence." << std::endl
            << "Node: " << std::endl
              << node << std::endl;
  }
  else
  {
    try
    {
      ret.resize(config.size());
      for(std::size_t i=0; i<node.size();i++)
      {
        if(!config[i].IsSequence())
        {
          what << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
            << "Tried to extract a ' "
              << boost::typeindex::type_id_with_cvr<decltype(T())>().pretty_name() 
                << "' but the node is not a sequence." << std::endl
                  << "Node: " << std::endl
                    << config << std::endl;
        }
        std::vector<T> v;
        ok = get_sequence(config[i],v,what);
        if(!ok)
        {
          what << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
            << "Error in the extraction of the element #" << i << ". Type of the sequence: "
              << boost::typeindex::type_id_with_cvr<decltype(std::vector<T>())>().pretty_name() 
                << "' but the node is not a sequence." << std::endl
                  << "Node: " << std::endl
                    << node << std::endl;
          break;
        }
        ret.push_back(v);
      }
    }
    CATCH(ret);
  }
  return ok;
}

template<typename T, std::size_t  N>
bool get_sequence(const YAML::Node& node, std::array<T,N>& ret, std::stringstream& what)
{
  bool ok = false;
  try
  {
    std::vector<T> tmp;
    ok = get_sequence(node,tmp,what);
    if(!ok || (tmp.size()==N))
    {
      what << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
        << "Tried to extract a ' "
          << boost::typeindex::type_id_with_cvr<decltype(T())>().pretty_name() 
            << "' ?error in size?" << std::endl
              << "Node: " << std::endl
                << node << std::endl;
    }
    else
    {
      std::copy_n(tmp.begin(), N, ret.begin());
      ok = true;
    }
  }
  CATCH(ret);

  return ok;
}

template<typename T, std::size_t N, std::size_t M>
bool get_sequence(const node_t& node, std::array<std::array<T,M>,N> ret, std::stringstream& what)
{
  bool ok = false;
  try
  {
    std::vector<std::vector<T>> tmp;
    ok = get_sequence(node, tmp, what);
    if(!ok || (tmp.size()!=N) || (tmp.front().size()!=M))
    {
      what << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
        << "Tried to extract a ' "
          << boost::typeindex::type_id_with_cvr<decltype(T())>().pretty_name() 
            << "' ?error in size?" << std::endl
              << "Node: " << std::endl
                << node << std::endl;
    }
    else
    {
      for(std::size_t i=0;i<N;i++)
      {
        std::copy_n(tmp.at(i).begin(), M, ret.at(i).begin());
      }
      ok = true;
    }
  }
  CATCH(ret);

  return ok;
}


/**
 * @brief 
 * 
 * @tparam Derived 
 * @param key 
 * @return Eigen::MatrixBase<Derived> 
 */
#if 1
template<typename Derived>
inline bool get_sequence(const YAML::Node& node, Eigen::MatrixBase<Derived> const & ret, std::stringstream& what)
{
  bool ok = false;
  Eigen::MatrixBase<Derived>& _ret = const_cast< Eigen::MatrixBase<Derived>& >(ret);
  YAML::Node config(node);

  int expected_rows = Eigen::MatrixBase<Derived>::RowsAtCompileTime;
  int expected_cols = Eigen::MatrixBase<Derived>::ColsAtCompileTime;
  bool should_be_a_vector = (expected_rows == 1 || expected_cols == 1);

  try
  {
    if (should_be_a_vector)
    {
      std::vector<double> vv;
      ok = get_sequence(config, vv, what);
      if(!ok)
      {
        what << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
          << "Tried to extract a vector from the "
                << "Node: " << std::endl
                  << node << std::endl;
        return false;
      }

      int dim = static_cast<int>(vv.size());
      if (!utils::resize(_ret, (expected_rows == 1 ? 1 : dim), (expected_rows == 1 ? dim : 1)))
      {
        what << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
            << "It was expected a Vector (" +
                                 std::to_string(expected_rows) + "x" + std::to_string(expected_cols) +
                                 ") while the param store a " + std::to_string(dim) + "-vector" << std::endl
                << "Node: " << std::endl
                  << node << std::endl;
        return false;
      }
      for (int i = 0; i < dim; i++)
      {
        _ret(i) = vv.at(static_cast<std::size_t>(i));
      }
    }
    else  // matrix expected
    {
      std::vector<std::vector<double>> vv;
      ok = get_sequence(node, vv, what);
      if(!ok)
      {
        std::cerr << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
          << "Tried to extract a vector from the "
                << "Node: " << std::endl
                  << node << std::endl;
        return false;
      }

      int rows = static_cast<int>(vv.size());
      int cols = static_cast<int>(vv.front().size());
      if (!utils::resize(_ret, rows, cols))
      {
        what << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
            << "It was expected a Vector (" +
                                 std::to_string(expected_rows) + "x" + std::to_string(expected_cols) +
                                 ") while the param store a " + std::to_string(vv.size()) + "-vector" << std::endl
                << "Node: " << std::endl
                  << node << std::endl;
        return false;
      }
      for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
          _ret(i, j) = vv.at(static_cast<int>(i)).at(static_cast<int>(j));
    }
  }
  catch (std::exception& e)
  {
    what << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
        << "Node: " << std::endl
          << node << std::endl
            << "What: " << std::endl
              << e.what() << std::endl;
    ok = false;
  }
  catch (...)
  {
    what  << __PRETTY_FUNCTION__ << ":" << __LINE__ << ": "
            << "Node: " << std::endl
              << node << std::endl
                << "What: " << std::endl
                  << "Wrong format" << std::endl;
    ok = false;
  }
  return true;
}
#endif

template<typename T>
inline bool get_map(const node_t& node, T& ret, std::stringstream& what)
{
  UNUSED(ret);
  what<< "The type ' "
        << boost::typeindex::type_id_with_cvr<decltype(T())>().pretty_name() 
          << "' is not supported. You must specilized your own 'get_map' template function" << std::endl
            << "' Node: " << std::endl
              << node  << std::endl;
  return false;
}

}
}

#endif  /* CNR_PARAM_INCLUDE_CNR_PARAM_IMPL_YAML_CNR_PARAM_YAML_CPP_IMPL */

