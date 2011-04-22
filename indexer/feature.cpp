#include "../base/SRC_FIRST.hpp"

#include "feature.hpp"
#include "feature_impl.hpp"
#include "feature_visibility.hpp"
#include "scales.hpp"
#include "geometry_coding.hpp"
#include "geometry_serialization.hpp"

#include "../defines.hpp" // just for file extensions

#include "../geometry/pointu_to_uint64.hpp"
#include "../geometry/rect2d.hpp"
#include "../geometry/region2d.hpp"

#include "../coding/byte_stream.hpp"

#include "../base/logging.hpp"
#include "../base/stl_add.hpp"

#include "../std/algorithm.hpp"

#include "../base/start_mem_debug.hpp"


///////////////////////////////////////////////////////////////////////////////////////////////////
// FeatureBuilder1 implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

FeatureBuilder1::FeatureBuilder1()
: m_Layer(0), m_bPoint(false), m_bLinear(false), m_bArea(false)
{
}

bool FeatureBuilder1::IsGeometryClosed() const
{
  return (m_Geometry.size() > 2 && m_Geometry.front() == m_Geometry.back());
}

void FeatureBuilder1::SetCenter(m2::PointD const & p)
{
  m_Center = p;
  m_bPoint = true;
  m_LimitRect.Add(p);
}

void FeatureBuilder1::AddPoint(m2::PointD const & p)
{
  m_Geometry.push_back(p);
  m_LimitRect.Add(p);
}

void FeatureBuilder1::SetAreaAddHoles(list<points_t> const & holes)
{
  m_bArea = true;
  m_Holes.clear();

  if (holes.empty()) return;

  m2::Region<m2::PointD> rgn(m_Geometry.begin(), m_Geometry.end());

  for (list<points_t>::const_iterator i = holes.begin(); i != holes.end(); ++i)
  {
    ASSERT ( !i->empty(), () );

    if (rgn.Contains(i->front()))
      m_Holes.push_back(*i);
  }
}

void FeatureBuilder1::AddName(string const & name)
{
  m_Name = name;
}

bool FeatureBuilder1::IsTypeExist(uint32_t t) const
{
  return (find(m_Types.begin(), m_Types.end(), t) != m_Types.end());
}

bool FeatureBuilder1::AssignType_SetDifference(vector<uint32_t> const & diffTypes)
{
  vector<uint32_t> src;
  src.swap(m_Types);

  sort(src.begin(), src.end());
  set_difference(src.begin(), src.end(), diffTypes.begin(), diffTypes.end(), back_inserter(m_Types));

  return !m_Types.empty();
}

void FeatureBuilder1::AddLayer(int32_t layer)
{
  int const bound = 10;
  if (layer < -bound) layer = -bound;
  else if (layer > bound) layer = bound;
  m_Layer = layer;
}

FeatureBase FeatureBuilder1::GetFeatureBase() const
{
  CHECK ( CheckValid(), () );

  FeatureBase f;
  f.SetHeader(GetHeader());

  f.m_Layer = m_Layer;
  for (size_t i = 0; i < m_Types.size(); ++i)
    f.m_Types[i] = m_Types[i];
  f.m_LimitRect = m_LimitRect;
  f.m_Name = m_Name;

  f.m_bTypesParsed = f.m_bCommonParsed = true;

  return f;
}

namespace
{
  bool is_equal(double d1, double d2)
  {
    //return my::AlmostEqual(d1, d2, 100000000);
    return (fabs(d1 - d2) < MercatorBounds::GetCellID2PointAbsEpsilon());
  }

  bool is_equal(m2::PointD const & p1, m2::PointD const & p2)
  {
    return p1.EqualDxDy(p2, MercatorBounds::GetCellID2PointAbsEpsilon());
  }

  bool is_equal(m2::RectD const & r1, m2::RectD const & r2)
  {
    return (is_equal(r1.minX(), r2.minX()) &&
            is_equal(r1.minY(), r2.minY()) &&
            is_equal(r1.maxX(), r2.maxX()) &&
            is_equal(r1.maxY(), r2.maxY()));
  }

  bool is_equal(vector<m2::PointD> const & v1, vector<m2::PointD> const & v2)
  {
    if (v1.size() != v2.size())
      return false;

    for (size_t i = 0; i < v1.size(); ++i)
      if (!is_equal(v1[i], v2[i]))
        return false;

    return true;
  }
}

bool FeatureBuilder1::operator == (FeatureBuilder1 const & fb) const
{
  if (m_Types != fb.m_Types ||
      m_Layer != fb.m_Layer ||
      m_Name != fb.m_Name ||
      m_bPoint != fb.m_bPoint ||
      m_bLinear != fb.m_bLinear ||
      m_bArea != fb.m_bArea)
  {
    return false;
  }

  if (m_bPoint && !is_equal(m_Center, fb.m_Center))
    return false;

  if (!is_equal(m_LimitRect, fb.m_LimitRect))
    return false;

  if (!is_equal(m_Geometry, fb.m_Geometry))
    return false;

  if (m_Holes.size() != fb.m_Holes.size())
    return false;

  list<points_t>::const_iterator i = m_Holes.begin();
  list<points_t>::const_iterator j = fb.m_Holes.begin();
  for (; i != m_Holes.end(); ++i, ++j)
    if (!is_equal(*i, *j))
      return false;

  return true;
}

bool FeatureBuilder1::CheckValid() const
{
  CHECK(!m_Types.empty() && m_Types.size() <= m_maxTypesCount, ());

  CHECK(m_Layer >= -10 && m_Layer <= 10, ());

  CHECK(m_bPoint || m_bLinear || m_bArea, ());

  CHECK(!m_bLinear || m_Geometry.size() >= 2, ());

  CHECK(!m_bArea || m_Geometry.size() >= 3, ());

  CHECK(m_Holes.empty() || m_bArea, ());

  for (list<points_t>::const_iterator i = m_Holes.begin(); i != m_Holes.end(); ++i)
    CHECK(i->size() >= 3, ());

  return true;
}

uint8_t FeatureBuilder1::GetHeader() const
{
  uint8_t header = static_cast<uint8_t>(m_Types.size());

  if (!m_Name.empty())
    header |= FeatureBase::HEADER_HAS_NAME;

  if (m_Layer != 0)
    header |= FeatureBase::HEADER_HAS_LAYER;

  if (m_bPoint)
    header |= FeatureBase::HEADER_HAS_POINT;

  if (m_bLinear)
    header |= FeatureBase::HEADER_IS_LINE;

  if (m_bArea)
    header |= FeatureBase::HEADER_IS_AREA;

  return header;
}

void FeatureBuilder1::SerializeBase(buffer_t & data, m2::PointU const & basePoint) const
{
  PushBackByteSink<buffer_t> sink(data);

  WriteToSink(sink, GetHeader());

  for (size_t i = 0; i < m_Types.size(); ++i)
    WriteVarUint(sink, m_Types[i]);

  if (m_Layer != 0)
    WriteVarInt(sink, m_Layer);

  if (!m_Name.empty())
  {
    WriteVarUint(sink, m_Name.size() - 1);
    sink.Write(&m_Name[0], m_Name.size());
  }

  if (m_bPoint)
    WriteVarUint(sink, EncodeDelta(PointD2PointU(m_Center.x, m_Center.y), basePoint));
}

void FeatureBuilder1::Serialize(buffer_t & data) const
{
  CHECK ( CheckValid(), () );

  data.clear();

  SerializeBase(data, m2::PointU(0, 0));

  PushBackByteSink<buffer_t> sink(data);

  if (m_bLinear || m_bArea)
    serial::SaveOuterPath(m_Geometry, 0, sink);

  if (m_bArea)
  {
    WriteVarUint(sink, uint32_t(m_Holes.size()));

    for (list<points_t>::const_iterator i = m_Holes.begin(); i != m_Holes.end(); ++i)
      serial::SaveOuterPath(*i, 0, sink);
  }

  // check for correct serialization
#ifdef DEBUG
  buffer_t tmp(data);
  FeatureBuilder1 fb;
  fb.Deserialize(tmp);
  ASSERT ( fb == *this, () );
#endif
}

namespace
{
  template <class TCont>
  void CalcRect(TCont const & points, m2::RectD & rect)
  {
    for (size_t i = 0; i < points.size(); ++i)
      rect.Add(points[i]);
  }
}

void FeatureBuilder1::Deserialize(buffer_t & data)
{
  FeatureBase f;
  f.Deserialize(data, 0, 0);
  f.InitFeatureBuilder(*this);

  ArrayByteSource src(f.DataPtr() + f.m_Header2Offset);

  if (m_bLinear || m_bArea)
  {
    serial::LoadOuterPath(src, 0, m_Geometry);
    CalcRect(m_Geometry, m_LimitRect);
  }

  if (m_bArea)
  {
    uint32_t const count = ReadVarUint<uint32_t>(src);
    for (uint32_t i = 0; i < count; ++i)
    {
      m_Holes.push_back(points_t());
      serial::LoadOuterPath(src, 0, m_Holes.back());
    }
  }

  CHECK ( CheckValid(), () );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FeatureBuilderGeomRef implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

bool FeatureBuilder2::IsDrawableInRange(int lowS, int highS) const
{
  if (!m_Geometry.empty())
  {
    FeatureBase const fb = GetFeatureBase();

    while (lowS <= highS)
      if (feature::IsDrawableForIndex(fb, lowS++))
        return true;
  }

  return false;
}

bool FeatureBuilder2::PreSerialize(buffers_holder_t const & data)
{
  // make flags actual before header serialization
  if (data.m_ptsMask == 0 && data.m_innerPts.empty())
    m_bLinear = false;

  if (data.m_trgMask == 0 && data.m_innerTrg.empty())
    m_bArea = false;

  // we don't need empty features without geometry
  return base_type::PreSerialize();
}

namespace
{
  template <class TSink> class BitSink
  {
    TSink & m_sink;
    uint8_t m_pos;
    uint8_t m_current;

  public:
    BitSink(TSink & sink) : m_sink(sink), m_pos(0), m_current(0) {}

    void Finish()
    {
      if (m_pos > 0)
      {
        WriteToSink(m_sink, m_current);
        m_pos = 0;
        m_current = 0;
      }
    }

    void Write(uint8_t value, uint8_t count)
    {
      ASSERT_LESS ( count, 9, () );
      ASSERT_EQUAL ( value >> count, 0, () );

      if (m_pos + count > 8)
        Finish();

      m_current |= (value << m_pos);
      m_pos += count;
    }
  };
}

void FeatureBuilder2::Serialize(buffers_holder_t & data, int64_t base)
{
  data.m_buffer.clear();

  // header data serialization
  SerializeBase(data.m_buffer, m2::Uint64ToPointU(static_cast<uint64_t>(base)));

  PushBackByteSink<buffer_t> sink(data.m_buffer);

  uint8_t const ptsCount = static_cast<uint8_t>(data.m_innerPts.size());
  uint8_t trgCount = static_cast<uint8_t>(data.m_innerTrg.size());
  if (trgCount > 0)
  {
    ASSERT_GREATER ( trgCount, 2, () );
    trgCount -= 2;
  }

  BitSink< PushBackByteSink<buffer_t> > bitSink(sink);

  if (m_bLinear)
  {
    bitSink.Write(ptsCount, 4);
    if (ptsCount == 0)
      bitSink.Write(data.m_ptsMask, 4);
  }

  if (m_bArea)
  {
    bitSink.Write(trgCount, 4);
    if (trgCount == 0)
      bitSink.Write(data.m_trgMask, 4);
  }

  bitSink.Finish();

  if (m_bLinear)
  {
    if (ptsCount > 0)
    {
      if (ptsCount > 2)
      {
        uint32_t v = data.m_ptsSimpMask;
        int const count = (ptsCount - 2 + 3) / 4;
        for (int i = 0; i < count; ++i)
        {
          WriteToSink(sink, static_cast<uint8_t>(v));
          v >>= 8;
        }
      }

      serial::SaveInnerPath(data.m_innerPts, base, sink);
    }
    else
    {
      // offsets was pushed from high scale index to low
      reverse(data.m_ptsOffset.begin(), data.m_ptsOffset.end());
      serial::WriteVarUintArray(data.m_ptsOffset, sink);
    }
  }

  if (m_bArea)
  {
    if (trgCount > 0)
      serial::SaveInnerTriangles(data.m_innerTrg, base, sink);
    else
    {
      // offsets was pushed from high scale index to low
      reverse(data.m_trgOffset.begin(), data.m_trgOffset.end());
      serial::WriteVarUintArray(data.m_trgOffset, sink);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FeatureBase implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

void FeatureBase::Deserialize(buffer_t & data, uint32_t offset, int64_t base)
{
  m_Offset = offset;
  m_Data.swap(data);

  m_base = base;

  m_CommonOffset = m_Header2Offset = 0;
  m_bTypesParsed = m_bCommonParsed = false;

  m_Layer = 0;
  m_Name.clear();
  m_LimitRect = m2::RectD::GetEmptyRect();
}

uint32_t FeatureBase::CalcOffset(ArrayByteSource const & source) const
{
  return static_cast<uint32_t>(static_cast<char const *>(source.Ptr()) - DataPtr());
}

void FeatureBase::SetHeader(uint8_t h)
{
  ASSERT ( m_Offset == 0, (m_Offset) );
  m_Data.resize(1);
  m_Data[0] = h;
}

void FeatureBase::ParseTypes() const
{
  ASSERT(!m_bTypesParsed, ());

  ArrayByteSource source(DataPtr() + m_TypesOffset);
  for (size_t i = 0; i < GetTypesCount(); ++i)
    m_Types[i] = ReadVarUint<uint32_t>(source);

  m_bTypesParsed = true;
  m_CommonOffset = CalcOffset(source);
}

void FeatureBase::ParseCommon() const
{
  ASSERT(!m_bCommonParsed, ());
  if (!m_bTypesParsed)
    ParseTypes();

  ArrayByteSource source(DataPtr() + m_CommonOffset);

  uint8_t const h = Header();

  if (h & HEADER_HAS_LAYER)
    m_Layer = ReadVarInt<int32_t>(source);

  if (h & HEADER_HAS_NAME)
  {
    m_Name.resize(ReadVarUint<uint32_t>(source) + 1);
    source.Read(&m_Name[0], m_Name.size());
  }

  if (h & HEADER_HAS_POINT)
  {
    CoordPointT center = PointU2PointD(DecodeDelta(ReadVarUint<uint64_t>(source),
                                                   m2::Uint64ToPointU(m_base)));
    m_Center = m2::PointD(center.first, center.second);
    m_LimitRect.Add(m_Center);
  }

  m_bCommonParsed = true;
  m_Header2Offset = CalcOffset(source);
}

void FeatureBase::ParseAll() const
{
  if (!m_bCommonParsed)
    ParseCommon();
}

string FeatureBase::DebugString() const
{
  ASSERT(m_bCommonParsed, ());

  string res("FEATURE: ");
  res +=  "'" + m_Name + "' ";

  for (size_t i = 0; i < GetTypesCount(); ++i)
    res += "Type:" + debug_print(m_Types[i]) + " ";

  res += "Layer:" + debug_print(m_Layer) + " ";

  if (Header() & HEADER_HAS_POINT)
    res += "Center:" + debug_print(m_Center) + " ";

  return res;
}

void FeatureBase::InitFeatureBuilder(FeatureBuilder1 & fb) const
{
  ParseAll();

  fb.AddTypes(m_Types, m_Types + GetTypesCount());
  fb.AddLayer(m_Layer);
  fb.AddName(m_Name);

  uint8_t const h = Header();

  if (h & HEADER_HAS_POINT)
    fb.SetCenter(m_Center);

  if (h & HEADER_IS_LINE)
    fb.SetLinear();

  if (h & HEADER_IS_AREA)
  {
    list<vector<m2::PointD> > l;
    fb.SetAreaAddHoles(l);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FeatureType implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

FeatureType::FeatureType(read_source_t & src)
{
  Deserialize(src);
}

void FeatureType::Deserialize(read_source_t & src)
{
  m_cont = &src.m_cont;
  m_header = &src.m_header;

  m_Points.clear();
  m_Triangles.clear();

  m_bHeader2Parsed = m_bPointsParsed = m_bTrianglesParsed = false;
  m_ptsSimpMask = 0;

  m_InnerStats.MakeZero();

  base_type::Deserialize(src.m_data, src.m_offset, m_header->GetBase());
}

namespace
{
    uint32_t const kInvalidOffset = uint32_t(-1);
}

int FeatureType::GetScaleIndex(int scale) const
{
  int const count = m_header->GetScalesCount();
  if (scale == -1) return count-1;

  for (int i = 0; i < count; ++i)
    if (scale <= m_header->GetScale(i))
      return i;
  return -1;
}

int FeatureType::GetScaleIndex(int scale, offsets_t const & offsets) const
{
  if (scale == -1)
  {
    // Choose the best geometry for the last visible scale.
    int i = offsets.size()-1;
    while (i >= 0 && offsets[i] == kInvalidOffset) --i;
    if (i >= 0)
      return i;
    else
      CHECK ( false, ("Feature should have any geometry ...") );
  }
  else
  {
    for (size_t i = 0; i < m_header->GetScalesCount(); ++i)
      if (scale <= m_header->GetScale(i))
      {
        if (offsets[i] != kInvalidOffset)
          return i;
        else
          break;
      }
  }

  return -1;
}

namespace
{
  template <class TCont>
  void Points2String(string & s, TCont const & points)
  {
    for (size_t i = 0; i < points.size(); ++i)
      s += debug_print(points[i]) + " ";
  }
}

string FeatureType::DebugString(int scale) const
{
  ParseAll(scale);

  string s = base_type::DebugString();

  s += "Points:";
  Points2String(s, m_Points);

  s += "Triangles:";
  Points2String(s, m_Triangles);

  return s;
}

bool FeatureType::IsEmptyGeometry(int scale) const
{
  ParseAll(scale);

  switch (GetFeatureType())
  {
  case FEATURE_TYPE_AREA: return m_Triangles.empty();
  case FEATURE_TYPE_LINE: return m_Points.empty();
  default:
    ASSERT ( Header() & HEADER_HAS_POINT, () );
    return false;
  }
}

m2::RectD FeatureType::GetLimitRect(int scale) const
{
  ParseAll(scale);

  if (m_Triangles.empty() && m_Points.empty() && (Header() & HEADER_HAS_POINT) == 0)
  {
    // This function is called during indexing, when we need
    // to check visibility according to feature sizes.
    // So, if no geometry for this scale, assume that rect has zero dimensions.
    m_LimitRect = m2::RectD(0, 0, 0, 0);
  }

  return m_LimitRect;
}

namespace
{
  class BitSource
  {
    char const * m_ptr;
    uint8_t m_pos;

  public:
    BitSource(char const * p) : m_ptr(p), m_pos(0) {}

    uint8_t Read(uint8_t count)
    {
      ASSERT_LESS ( count, 9, () );

      uint8_t v = *m_ptr;
      v >>= m_pos;
      v &= ((1 << count) - 1);

      m_pos += count;
      if (m_pos >= 8)
      {
        ASSERT_EQUAL ( m_pos, 8, () );
        ++m_ptr;
        m_pos = 0;
      }

      return v;
    }

    char const * RoundPtr()
    {
      if (m_pos > 0)
      {
        ++m_ptr;
        m_pos = 0;
      }
      return m_ptr;
    }
  };

  template <class TSource> uint8_t ReadByte(TSource & src)
  {
    return ReadPrimitiveFromSource<uint8_t>(src);
  }
}

void FeatureType::ParseHeader2() const
{
  ASSERT(!m_bHeader2Parsed, ());
  if (!m_bCommonParsed)
    ParseCommon();

  uint8_t ptsCount, ptsMask, trgCount, trgMask;

  uint8_t const commonH = Header();
  BitSource bitSource(DataPtr() + m_Header2Offset);

  if (commonH & HEADER_IS_LINE)
  {
    ptsCount = bitSource.Read(4);
    if (ptsCount == 0)
      ptsMask = bitSource.Read(4);
    else
    {
      ASSERT_GREATER ( ptsCount, 1, () );
    }
  }

  if (commonH & HEADER_IS_AREA)
  {
    trgCount = bitSource.Read(4);
    if (trgCount == 0)
      trgMask = bitSource.Read(4);
  }

  ArrayByteSource src(bitSource.RoundPtr());

  if (commonH & HEADER_IS_LINE)
  {
    if (ptsCount > 0)
    {
      int const count = (ptsCount - 2 + 3) / 4;
      ASSERT_LESS ( count, 4, () );

      for (int i = 0; i < count; ++i)
      {
        uint32_t mask = ReadByte(src);
        m_ptsSimpMask += (mask << (i << 3));
      }

      char const * start = static_cast<char const *>(src.Ptr());

      src = ArrayByteSource(serial::LoadInnerPath(src.Ptr(), ptsCount, m_base, m_Points));

      m_InnerStats.m_Points = static_cast<char const *>(src.Ptr()) - start;
    }
    else
      ReadOffsets(src, ptsMask, m_ptsOffsets);
  }

  if (commonH & HEADER_IS_AREA)
  {
    if (trgCount > 0)
    {
      trgCount += 2;

      char const * start = static_cast<char const *>(src.Ptr());

      points_t points;
      src = ArrayByteSource(serial::LoadInnerTriangles(src.Ptr(), trgCount, m_base, points));

      m_InnerStats.m_Strips = static_cast<char const *>(src.Ptr()) - start;

      for (uint8_t i = 2; i < trgCount; ++i)
      {
        m_Triangles.push_back(points[i-2]);
        m_Triangles.push_back(points[i-1]);
        m_Triangles.push_back(points[i]);
      }
    }
    else
      ReadOffsets(src, trgMask, m_trgOffsets);
  }

  m_bHeader2Parsed = true;
  m_InnerStats.m_Size = static_cast<char const *>(src.Ptr()) - DataPtr();
}

uint32_t FeatureType::ParseGeometry(int scale) const
{
  ASSERT(!m_bPointsParsed, ());
  if (!m_bHeader2Parsed)
    ParseHeader2();

  uint32_t sz = 0;
  if (Header() & HEADER_IS_LINE)
  {
    if (m_Points.empty())
    {
      // outer geometry
      int const ind = GetScaleIndex(scale, m_ptsOffsets);
      if (ind != -1)
      {
        ReaderSource<FileReader> src(
              m_cont->GetReader(feature::GetTagForIndex(GEOMETRY_FILE_TAG, ind)));
        src.Skip(m_ptsOffsets[ind]);
        serial::LoadOuterPath(src, m_base, m_Points);

        sz = static_cast<uint32_t>(src.Pos() - m_ptsOffsets[ind]);
      }
    }
    else
    {
      // filter inner geometry

      size_t const count = m_Points.size();
      points_t points;
      points.reserve(count);

      uint32_t const scaleIndex = GetScaleIndex(scale);
      ASSERT_LESS ( scaleIndex, m_header->GetScalesCount(), () );

      points.push_back(m_Points.front());
      for (size_t i = 1; i < count-1; ++i)
      {
        // check for point visibility in needed scaleIndex
        if (((m_ptsSimpMask >> (2*(i-1))) & 0x3) <= scaleIndex)
          points.push_back(m_Points[i]);
      }
      points.push_back(m_Points.back());

      m_Points.swap(points);
    }

    CalcRect(m_Points, m_LimitRect);
  }

  m_bPointsParsed = true;
  return sz;
}

uint32_t FeatureType::ParseTriangles(int scale) const
{
  ASSERT(!m_bTrianglesParsed, ());
  if (!m_bHeader2Parsed)
    ParseHeader2();

  uint32_t sz = 0;
  if (Header() & HEADER_IS_AREA)
  {
    if (m_Triangles.empty())
    {
      uint32_t const ind = GetScaleIndex(scale, m_trgOffsets);
      if (ind != -1)
      {
        ReaderSource<FileReader> src(
              m_cont->GetReader(feature::GetTagForIndex(TRIANGLE_FILE_TAG, ind)));
        src.Skip(m_trgOffsets[ind]);
        serial::LoadOuterTriangles(src, m_base, m_Triangles);

        sz = static_cast<uint32_t>(src.Pos() - m_trgOffsets[ind]);
      }
    }

    CalcRect(m_Triangles, m_LimitRect);
  }

  m_bTrianglesParsed = true;
  return sz;
}

void FeatureType::ReadOffsets(ArrayByteSource & src, uint8_t mask, offsets_t & offsets) const
{
  ASSERT_GREATER ( mask, 0, () );

  int index = 0;
  while (mask > 0)
  {
    ASSERT_LESS ( index, m_header->GetScalesCount(), () );
    offsets[index++] = (mask & 0x01) ? ReadVarUint<uint32_t>(src) : kInvalidOffset;
    mask = mask >> 1;
  }
}

void FeatureType::ParseAll(int scale) const
{
  if (!m_bPointsParsed)
    ParseGeometry(scale);

  if (!m_bTrianglesParsed)
    ParseTriangles(scale);
}

FeatureType::geom_stat_t FeatureType::GetGeometrySize(int scale) const
{
  uint32_t sz = ParseGeometry(scale);
  if (sz == 0 && !m_Points.empty())
    sz = m_InnerStats.m_Points;

  return geom_stat_t(sz, m_Points.size());
}

FeatureType::geom_stat_t FeatureType::GetTrianglesSize(int scale) const
{
  uint32_t sz = ParseTriangles(scale);
  if (sz == 0 && !m_Triangles.empty())
    sz = m_InnerStats.m_Strips;

  return geom_stat_t(sz, m_Triangles.size());
}
