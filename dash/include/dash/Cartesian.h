/* 
 * dash-lib/Cartesian.h
 *
 * author(s): Karl Fuerlinger, LMU Munich 
 */
/* @DASH_HEADER@ */

#ifndef CARTESIAN_H_INCLUDED
#define CARTESIAN_H_INCLUDED

#include <array>
#include <algorithm>
#include <cassert>
#include <cstring>

#include <dash/Enums.h>
#include <dash/Exception.h>

namespace dash {

/// Forward-declaration
template<size_t NumDimensions_, MemArrange arr> class Pattern;

/**
 * Translates between linar and cartesian coordinates
 * TODO: Could be renamed to MemoryOrder?
 */
template<
  int NumDimensions,
  typename SizeType      = size_t,
  MemArrange Arrangement = ROW_MAJOR >
class CartCoord {
public:
  template<size_t NumDimensions_, MemArrange arr> friend class Pattern;

protected:
  /// Number of elements in the cartesian space spanned by this coordinate.
  SizeType m_size;
  /// Number of dimensions of this coordinate.
  size_t m_ndim;
  /// Extents of the coordinate by dimension.
  SizeType m_extent[NumDimensions];
  /// Cumulative index offsets of the coordinate by dimension.
  SizeType m_offset[NumDimensions];

public:
  /**
   * Default constructor, creates a cartesian coordinate of extent 0
   * in all dimensions.
   */
  CartCoord()
  : m_size(0),
    m_ndim(NumDimensions) {
    for(auto i = 0; i < NumDimensions; i++) {
      m_extent[i] = 0;
      m_offset[i] = 0;
    }
  }

  /**
   * Constructor, creates a cartesian coordinate of given extents in
   * all dimensions.
   */
  CartCoord(::std::array<SizeType, NumDimensions> extents)
  : m_size(0),
    m_ndim(NumDimensions) {
    static_assert(
      extents.size() == NumDimensions,
      "Invalid number of arguments");
    ::std::copy(extents.begin(), extents.end(), m_extent);
    m_size = 1;
    for(auto i = 0; i < NumDimensions; i++ ) {
      if (m_extent[i] <= 0) {
        DASH_THROW(
          dash::exception::OutOfBounds,
          "Coordinates for CartCoord() must be greater than 0");
      }
      // TODO: assert( std::numeric_limits<SizeType>::max()/m_extent[i]);
      m_size *= m_extent[i];
    }
   
    if (Arrangement == COL_MAJOR) {
      m_offset[NumDimensions-1] = 1;
      for(auto i = NumDimensions-2; i >= 0; --i) {
        m_offset[i] = m_offset[i+1] * m_extent[i+1];
      }
    } else if (Arrangement == ROW_MAJOR) {
      m_offset[0] = 1;
      for(auto i = 1; i < NumDimensions; ++i) {
        m_offset[i] = m_offset[i-1] * m_extent[i-1];
      }
    }
  }

  /**
   * Constructor, creates a cartesian coordinate of given extents in
   * all dimensions.
   */
  template<typename... Args>
  CartCoord(Args... args) 
  : m_size(0),
    m_extent { SizeType(args)... },
    m_ndim(NumDimensions) {
    static_assert(
      sizeof...(Args) == NumDimensions,
      "Invalid number of arguments");
    m_size = 1;
    for(auto i = 0; i < NumDimensions; i++ ) {
      if (m_extent[i] <= 0) {
        DASH_THROW(
          dash::exception::OutOfBounds,
          "Coordinates for CartCoord() must be greater than 0");
      }
      // TODO: assert( std::numeric_limits<SizeType>::max()/m_extent[i]);
      m_size *= m_extent[i];
    }
   
    if (Arrangement == COL_MAJOR) {
      m_offset[NumDimensions-1] = 1;
      for(auto i = NumDimensions-2; i >= 0; --i) {
        m_offset[i] = m_offset[i+1] * m_extent[i+1];
      }
    } else if (Arrangement == ROW_MAJOR) {
      m_offset[0] = 1;
      for(auto i = 1; i < NumDimensions; ++i) {
        m_offset[i] = m_offset[i-1] * m_extent[i-1];
      }
    }
  }
  
  /**
   * The coordinate's rank.
   *
   * \return The number of dimensions in the coordinate
   */
  int rank() const {
    return NumDimensions;
  }
  
  /**
   * The number of discrete elements within the space spanned by the
   * coordinate.
   *
   * \return The number of discrete elements in the coordinate's space
   */
  SizeType size() const {
    return m_size;
  }
  
  /**
   * The coordinate's extent in the given dimension.
   *
   * \param  dim  The dimension in the coordinate
   * \return      The extent in the given dimension
   */
  SizeType extent(SizeType dim) const {
    assert(dim < NumDimensions);
    if (dim >= NumDimensions) {
      // Dimension out of bounds:
      DASH_THROW(
        dash::exception::OutOfBounds,
        "Given dimension " << dim <<
        " for CartCoord::extent() is out of bounds" <<
        " (" << NumDimensions << ")");
    }
    return m_extent[dim];
  }
  
  /**
   * Convert the given coordinates to a linear index.
   *
   * \param  args  An argument list consisting of the coordinates, ordered
   *               by dimension (x, y, z, ...)
   */
  template<typename... Args>
  SizeType at(Args... args) const {
    static_assert(
      sizeof...(Args) == NumDimensions,
      "Invalid number of arguments");
    ::std::array<SizeType, NumDimensions> pos = { SizeType(args) ... };
    return at(pos);
  }
  
  /**
   * Convert the given coordinates to a linear index.
   *
   * \param  pos  An array containing the coordinates, ordered by
   * dimension (x, y, z, ...)
   */
  SizeType at(std::array<SizeType, NumDimensions> pos) const {
    static_assert(
      pos.size() == NumDimensions,
      "Invalid number of arguments");
    SizeType offs = 0;
    for (int i = 0; i < NumDimensions; i++) {
      if (pos[i] >= m_extent[i]) {
        // Coordinate out of bounds:
        DASH_THROW(
          dash::exception::OutOfBounds,
          "Given coordinate " << pos[i] <<
          " for CartCoord::at() is out of bounds");
      }
      offs += m_offset[i] * pos[i];
    }
    return offs;
  }
  
  SizeType at(
    ::std::array<SizeType, NumDimensions> pos,
    ::std::array<SizeType, NumDimensions> cyclicfix) const {
    static_assert(
      pos.size() == NumDimensions,
      "Invalid number of arguments");
    static_assert(
      cyclicfix.size() == NumDimensions,
      "Invalid number of arguments");
    SizeType offs = 0;
    for (int i = 0; i < NumDimensions; i++) {
      // omit NONE distribution
      if (pos[i] != -1) { 
        offs += pos[i] * (m_offset[i] + cyclicfix[i]);
      }
    }
    return offs;
  }
  
  /**
   * Convert given linear offset (index) to cartesian coordinates.
   * Inverse of \c at(...).
   */
  std::array<SizeType, NumDimensions> coords(SizeType offs) const {
    if (offs >= m_size) {
      // Index out of bounds:
      DASH_THROW(
        dash::exception::OutOfBounds,
        "Given index " << offs <<
        " for CartCoord::coords() is out of bounds");
    }
    ::std::array<SizeType, NumDimensions> pos;
    if (Arrangement == COL_MAJOR) {
      for(int i = 0; i < NumDimensions; ++i) {
        pos[i] = offs / m_offset[i];
        offs   = offs % m_offset[i];
      }
    } else if (Arrangement == ROW_MAJOR) {
      for(int i = NumDimensions-1; i >= 0; --i) {
        pos[i] = offs / m_offset[i];
        offs   = offs % m_offset[i];
      }
    }
    return pos;
  }
  
  /**
   * Accessor for dimension 1 (x), enabled for dimensionality > 0.
   */
  template<int U = NumDimensions>
  typename std::enable_if< (U > 0), SizeType >::type x(SizeType offs) const {
    return coords(offs)[0];
  }
  
  /**
   * Accessor for dimension 2 (y), enabled for dimensionality > 1.
   */
  template<int U = NumDimensions>
  typename std::enable_if< (U > 1), SizeType >::type y(SizeType offs) const {
    return coords(offs)[1];
  }
  
  /**
   * Accessor for dimension 3 (z), enabled for dimensionality > 2.
   */
  template<int U = NumDimensions>
  typename std::enable_if< (U > 2), SizeType >::type z(SizeType offs) const {
    return coords(offs)[2];
  }
};

} // namespace dash

#endif /* CARTESIAN_H_INCLUDED */

