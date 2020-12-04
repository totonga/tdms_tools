/**
 * @author Andreas Krantz (totonga@gmail.com)
 * @brief Write the internal structure of an NI TDMS file into an XML file to make it human readable
 * @version 0.1
 * @date 2020-11-29
 * @copyright MIT License
**/

#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <stack>
#include <string>

namespace {

#pragma pack(push,1)

  struct SgmtHeader
  {
    char tag[4];
    struct toc_struct{
      bool reserved_1 : 1;
      bool MetaData : 1;
      bool NewObjList : 1;
      bool RawData : 1;
      bool reserved_2 : 1;
      bool InterleavedData : 1;
      bool BigEndian : 1;
      bool DAQmxRawData : 1;
    } toc;
    char toc_unused[3];
  };

  // layout of 80 bit extended floating point number
  struct float80Struct_ {
    unsigned long long mantissa : 64;
    unsigned int exponent : 15;
    bool sign : 1;
  };

  using float80_ = unsigned char[10];
  using fixpoint128_ = unsigned char[16];

#pragma pack(pop)

  /**
   * @brief Enumeration to discriminate datatypes of channels and properties
   */
  enum tdmsDataType {
    tdmsTypeVoid = 0x0,
    tdmsTypeI8 = 0x1,
    tdmsTypeI16 = 0x2,
    tdmsTypeI32 = 0x3,
    tdmsTypeI64 = 0x4,
    tdmsTypeU8 = 0x5,
    tdmsTypeU16 = 0x6,
    tdmsTypeU32 = 0x7,
    tdmsTypeU64 = 0x8,
    tdmsTypeSingleFloat = 0x9,
    tdmsTypeDoubleFloat = 0xA,
    tdmsTypeExtendedFloat = 0xB,
    tdmsTypeSingleFloatWithUnit = 0x19,
    tdmsTypeDoubleFloatWithUnit = 0x1A,
    tdmsTypeExtendedFloatWithUnit = 0x1B,
    tdmsTypeString = 0x20,
    tdmsTypeBoolean = 0x21,
    tdmsTypeTimeStamp = 0x44,
    tdmsTypeFixedPoint = 0x4F,
    tdmsTypeComplexSingleFloat = 0x08000c,
    tdmsTypeComplexDoubleFloat = 0x10000d,
    tdmsTypeDAQmxRawData = 0xFFFFFFFF
  };

  /**
   * @brief Convert enum to string for logging purposes
   * 
   * @param datatype datatype to convert
   * @return human readable string representing the datatype enumeration
   */
  std::string get_tdms_data_type_as_string(const tdmsDataType datatype)
  {
    switch (datatype) {
    case tdmsTypeVoid: return "Void";
    case tdmsTypeI8: return "I8";
    case tdmsTypeI16: return "I16";
    case tdmsTypeI32: return "I32";
    case tdmsTypeI64: return "I64";
    case tdmsTypeU8: return "U8";
    case tdmsTypeU16: return "U16";
    case tdmsTypeU32: return "U32";
    case tdmsTypeU64: return "U64";
    case tdmsTypeSingleFloat: return "SingleFloat";
    case tdmsTypeDoubleFloat: return "DoubleFloat";
    case tdmsTypeExtendedFloat: return "ExtendedFloat";
    case tdmsTypeSingleFloatWithUnit: return "SingleFloatWithUnit";
    case tdmsTypeDoubleFloatWithUnit: return "DoubleFloatWithUnit";
    case tdmsTypeExtendedFloatWithUnit: return "ExtendedFloatWithUnit";
    case tdmsTypeString: return "String";
    case tdmsTypeBoolean: return "Boolean";
    case tdmsTypeTimeStamp: return "TimeStamp";
    case tdmsTypeFixedPoint: return "FixedPoint";
    case tdmsTypeComplexSingleFloat: return "ComplexSingleFloat";
    case tdmsTypeComplexDoubleFloat: return "ComplexDoubleFloat";
    case tdmsTypeDAQmxRawData: return "DAQmxRawData";
    default: return "Unknown";
    }
  }

  /**
   * @brief Get the size in bytes for a single value of a given data type 
   * 
   * @param datatype Data type to determine size of
   * @return size in bytes for a fixed size datatype. for string and daqmx zero is returned
   */
  std::size_t get_tdms_data_type_byte_size(const tdmsDataType datatype)
  {
    switch (datatype) {
    case tdmsTypeVoid: return 0;
    case tdmsTypeI8: return sizeof(int8_t);
    case tdmsTypeI16: return sizeof(int16_t);
    case tdmsTypeI32: return sizeof(int32_t);
    case tdmsTypeI64: return sizeof(int64_t);
    case tdmsTypeU8: return sizeof(uint8_t);
    case tdmsTypeU16: return sizeof(uint16_t);
    case tdmsTypeU32: return sizeof(uint32_t);
    case tdmsTypeU64: return sizeof(uint64_t);
    case tdmsTypeSingleFloat: return sizeof(float);
    case tdmsTypeDoubleFloat: return sizeof(double);
    case tdmsTypeExtendedFloat: return sizeof(float80_);
    case tdmsTypeSingleFloatWithUnit: return sizeof(float);
    case tdmsTypeDoubleFloatWithUnit: return sizeof(double);
    case tdmsTypeExtendedFloatWithUnit: return sizeof(float80_);
    case tdmsTypeString: return 0;
    case tdmsTypeBoolean: return sizeof(uint8_t);
    case tdmsTypeTimeStamp: return sizeof(int64_t) + sizeof(uint64_t);
    case tdmsTypeFixedPoint: return sizeof(fixpoint128_);
    case tdmsTypeComplexSingleFloat: return 2 * sizeof(float);
    case tdmsTypeComplexDoubleFloat: return 2 * sizeof(double);
    case tdmsTypeDAQmxRawData: return 0;
    default: return 0;
    }
  }

  /**
   * @brief class to do the file system access
   */
  class FileIo
  {
  public:
    /**
     * @brief Open the file in binary mode for read
     * 
     * @tparam PathType  std::string or std::wstring to allow utf16 usage on windows if needed
     * @param filepath  path of the tdms file
     */
    template<class PathType> FileIo(const PathType& filepath) : ifs_(filepath, std::ios::binary | std::ios::ate)
    {
      if (!ifs_) {
        throw std::logic_error("Failed to open file");
      }

      auto end = ifs_.tellg();
      ifs_.seekg(0, std::ios::beg);

      size_ = end - ifs_.tellg();
    }

    /**
     * @brief Read bytes at the current position
     * 
     * @param buffer  buffer of size count
     * @param count   number of bytes to be read
     * @exception throws std::logic_error if no bytes left 
     */
    void read_bytes(void* buffer, size_t count)
    {
      if (!ifs_.read((char*)buffer, count)) {
        throw std::logic_error("Failed to read bytes");
      }
    }

    /**
     * @brief Read bytes at the current position
     * 
     * @param buffer   buffer of size count
     * @param count   number of bytes to be read
     * @return true   if successfull 
     * @return false  if not enough bytes left in the file
     */
    bool read_no_throw(void* buffer, size_t count)
    {
      if (!ifs_.read((char*)buffer, count)) {
        return false;
      }
      return true;
    }

    /**
     * @brief Set current file read position to an position relative to start of file
     * 
     * @param pos position to be set
     */
    void seek(const uint64_t pos)
    {
      ifs_.seekg(pos, std::ios::beg);
    }

    /**
     * @brief Get the size of the file
     * 
     * @return return file size in bytes
     */
    uint64_t size() const
    {
      return size_;
    }

  private:
    std::ifstream ifs_;
    uint64_t size_{ 0 };
  };

  /**
   * @brief Wrapper for Segment reading to manage the swapping of endianess for numeric values
   */
  class SgmtFileIo
  {
  public:
    /**
     * @brief Wrapper for FileIo to automatically swap endianess of numeric data
     * 
     * @param fileIo  file reader
     * @param segment_is_stored_as_big_endian determine if this segment is stored big or little endian
     */
    SgmtFileIo(FileIo& fileIo, const bool segment_is_stored_as_big_endian) :
      fileIo_(fileIo), swapEndianess_(is_big_endian_os() != segment_is_stored_as_big_endian)
    {
    }

    /**
     * @brief Read a value and swap endianess if necessary
     * 
     * @tparam T  type of the value to be read
     * @param val value to be read
     */
    template<class T> void read_value(T& val)
    {
      fileIo_.read_bytes(static_cast<void*>(&val), sizeof(val));
      if (swapEndianess_) {
        swap_endianess(val);
      }
    }

    /**
     * @brief Read a string from segment. String is a uint32 containing the number of bytes stored
     *        in the utf8 string following
     * 
     * @param strVal string to be filled
     */
    void read_string(std::string& strVal)
    {
      uint32_t numberOfBytes{ 0 };
      read_value(numberOfBytes);
      strVal.resize(numberOfBytes);
      fileIo_.read_bytes(&strVal[0], numberOfBytes);
    }

  private:
    /**
     * @brief Determine if the operating system is big or little endian
     * 
     * @return true  if big endian system
     * @return false if little endian system
     */
    bool is_big_endian_os()
    {
      union {
        uint32_t i;
        char c[4];
      } int_union = { 0x01020304 };
      return int_union.c[0] == 1;
    }

    inline void swap_endianess(uint8_t *buffer, const size_t &byteCount)
    {
      const size_t swapCount(byteCount / 2);
      if (0 == swapCount) {
        return;
      }

      size_t endCount(byteCount - 1);
      size_t count(0);
      for (; count < swapCount; ++count, --endCount) {
        unsigned char ch = buffer[count];
        buffer[count] = buffer[endCount];
        buffer[endCount] = ch;
      }
    }

    template <typename T>
    inline void swap_endianess(T &value)
    {
      swap_endianess(reinterpret_cast<uint8_t *>(&value), sizeof(T));
    }

  private:
    FileIo& fileIo_;
    const bool swapEndianess_;
  };

  /**
   * @brief stores information describing the raw setup of a channel in a segment
   */
  class SgmtObjectRawInfo
  {
  public:
    /**
     * @brief Construct a new Sgmt Object Raw Info object
     * 
     * @param objPath                 object path representing the channel
     * @param rawDatatypeEnum         data type of the channel
     * @param rawDataArrayDimension   dimension of channel
     * @param rawDataNumberOfValues   number of values of a channel stored in a single chunk of a segment
     * @param totalSizeInByte         is only set if the channel contains strings
     */
    SgmtObjectRawInfo(const std::string& objPath, const tdmsDataType rawDatatypeEnum,
      const uint32_t rawDataArrayDimension, const uint64_t rawDataNumberOfValues,
      const uint64_t totalSizeInByte) :
      objPath_(objPath), datatype_(rawDatatypeEnum),
      dimension_(rawDataArrayDimension), number_of_values_(rawDataNumberOfValues),
      total_size_in_byte_(totalSizeInByte)
    {
    }

    SgmtObjectRawInfo()
    {}

    ~SgmtObjectRawInfo()
    {
    }

  public:
    std::string objPath_;
    tdmsDataType datatype_{ tdmsTypeVoid };
    uint32_t dimension_{ 1 };
    uint64_t number_of_values_{ 0LL };
    uint64_t total_size_in_byte_{ 0LL };
  };

  /**
   * @brief Vector to collect raw channel definitions contained in a segment 
   */
  using ObjectRawInfos = std::map<std::string, SgmtObjectRawInfo>;

  /**
   * @brief Simple logger to collect information found in TDMS file while parsing
   */
  class ContentLoggerXml
  {
    public:
      /**
       * @brief Construct a new Content Logger Xml object
       * 
       * @tparam T  std::string or std::wstring to enable utf8 and utf16 if necessary
       * @param filepath path to the xml file to be written
       */
      template<class T> ContentLoggerXml(const T& filepath) :
        ost_(filepath, std::ios::binary | std::ios::out | std::ios::trunc)
      {
        ost_.imbue(std::locale("C")); // make sure decimal point is dot
        ost_ << R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>)" << std::endl;
      }

      /**
       * @brief push a tag. Must be matched with an call to 'pop'
       * 
       * @param tag  name of the tag to push
       */
      void push(const char* tag)
      {
        ident();
        ost_ << "<" << tag << ">" << std::endl;
        open_.push(tag);
      }

      /**
       * @brief Add a name value pair
       * 
       * @param name  name of entry
       * @param val   value of entry
       */
      template<class T> void add(const char* name, const T& val)
      {
        add_with_ident(name, val);
      }

      /**
       * @brief Add a name value pair
       * 
       * @param name  name of entry
       * @param val   value of entry
       */
      void add(const char* name, const std::string& val)
      {
        std::string escaped_val = val;
        replace_in_string_<std::string>(escaped_val, "<", "&lt;");
        replace_in_string_<std::string>(escaped_val, ">", "&gt;");
        replace_in_string_<std::string>(escaped_val, "&", "&amp;");
        add_with_ident(name, val);
      }

      /**
       * @brief close an tag opened with 'push'
       */
      void pop()
      {
        std::string tag = open_.top(); open_.pop();
        ident();
        ost_ << "</" << tag << ">" << std::endl;
      }

    private:

      template<class T> void add_with_ident(const char* name, const T& val)
      {
        ident();
        ost_ << "<" << name << ">" << val << "</" << name << ">" << std::endl;
      }

      void ident()
      {
        for(auto i = open_.size(); i > 0; --i) {
          ost_ << "  ";
        }
      }

      template <class T> void replace_in_string_(T &str, const T &search_str, const T &replace_str)
      {
        const typename T::size_type search_str_size(search_str.size());
        if (0 == search_str_size) {
          return;
        }

        const typename T::size_type replace_str_size(replace_str.size());
        typename T::size_type curr_pos = 0;
        while (T::npos != (curr_pos = str.find(search_str, curr_pos))) {
          str.replace(curr_pos, search_str_size, replace_str);
          curr_pos += replace_str_size;
        }
      }

    private:
      std::ofstream ost_;
      std::stack<std::string> open_; 
  };

  /**
   * @brief dump the structure of a tdms file into a structure logger
   * 
   * @tparam PathType     std::string or std::wstring to support utf8 and utf16
   * @param tdmsFilePath  path of the tdms file
   * @param sl            logger to write a target file
   */
  template<class PathType> void log_tdms_file_structure(const PathType& tdmsFilePath, ContentLoggerXml& sl)
  {
    static_assert(1 == sizeof(bool), "bool is stored in byte");
    static_assert(10 == sizeof(float80_), "extended float needs 80bit");
    static_assert(16 == sizeof(fixpoint128_), "fix point needs 128bit");
    static_assert(8 == sizeof(SgmtHeader), "lead in size is not allowed to change");

    sl.push("file");
    sl.add("filepath", tdmsFilePath);

    FileIo fileIo(tdmsFilePath);
    const int64_t fileSize = fileIo.size();

    sl.add("size_in_byte", fileSize);
    sl.push("segments");

    ObjectRawInfos objectRawInfosAll; // collects all to lookup for "0x0 == raw_data_index"
    ObjectRawInfos objectRawInfosCurr; // will be resetted if new_obj_list is started
    
    long sgmtIndex{ 0 };
    int64_t next_segment_absolute_offset{ 0LL };
    for (;;++sgmtIndex) {
      const int64_t curr_segment_absolute_offset{ next_segment_absolute_offset };

      if (fileSize == curr_segment_absolute_offset) {
        break;
      }

      ///////////////////////////////////////////
      // read lead in
      fileIo.seek(curr_segment_absolute_offset);
      SgmtHeader sgmtHeader;
      if (!fileIo.read_no_throw(&sgmtHeader, sizeof(SgmtHeader))) {
        // No segment left
        break;
      }
      if ('T' != sgmtHeader.tag[0] || 'D' != sgmtHeader.tag[1] || 'S' != sgmtHeader.tag[2] || 'm' != sgmtHeader.tag[3]) {
        throw std::logic_error("Segment always starts with TDSm");
      }
      
      sl.push("segment");
      sl.add("index", sgmtIndex);
    
      SgmtFileIo sgmtFileIO(fileIo, sgmtHeader.toc.BigEndian);

      uint32_t tdms_version;
      sgmtFileIO.read_value(tdms_version);
      if (0x1269 != tdms_version) {
        throw std::logic_error("Only TDMS 2.0 supported by this code");
      }

      sl.add("version", tdms_version);
      sl.push("table_of_content");
      sl.add("meta_data", sgmtHeader.toc.MetaData);
      sl.add("new_obj_list", sgmtHeader.toc.NewObjList);
      sl.add("raw_data", sgmtHeader.toc.RawData);
      sl.add("big_endian", sgmtHeader.toc.BigEndian);
      sl.add("interleaved_data", sgmtHeader.toc.InterleavedData);
      sl.add("big_endian", sgmtHeader.toc.BigEndian);
      sl.add("daqmx_raw_data", sgmtHeader.toc.DAQmxRawData);
      sl.pop();

      uint64_t next_segment_offset;
      sgmtFileIO.read_value(next_segment_offset);
      sl.add("next_segment_offset", next_segment_offset);

      uint64_t raw_data_offset;
      sgmtFileIO.read_value(raw_data_offset);
      sl.add("raw_data_offset", raw_data_offset);
      ////////////////////////////////////////////////////////////////

      const size_t leadInSizeInByte{ sizeof(SgmtHeader) + sizeof(tdms_version) + sizeof(next_segment_offset) + sizeof(raw_data_offset) };

      // segment starts after lead in
      int64_t sgmtStartOffset = curr_segment_absolute_offset + leadInSizeInByte;
      if (0xFFFFFFFFFFFFFFFFLL == next_segment_offset) {
        next_segment_offset = fileSize - sgmtStartOffset;
      }
      next_segment_absolute_offset = sgmtStartOffset + next_segment_offset;
      uint64_t raw_data_absolute_offset_in_byte = sgmtStartOffset + raw_data_offset;
      sl.add("absolut_segment_offset", curr_segment_absolute_offset);
      sl.add("absolut_raw_data_offset", raw_data_absolute_offset_in_byte);
      sl.add("absolut_next_segment_byte_offset", next_segment_absolute_offset);

      if (sgmtHeader.toc.NewObjList) {
        objectRawInfosCurr.clear();
      }

      // If the segment contains no meta data at all (properties, index information, object list), this value will be 0
      if(raw_data_offset > 0) {

        uint32_t numberOfNewObjects{ 0 };
        sgmtFileIO.read_value(numberOfNewObjects);
        sl.add("objects_count", numberOfNewObjects);
        sl.push("objects");
        std::string objPath;
        for (uint32_t objIndex = 0; objIndex < numberOfNewObjects; ++objIndex) {
          sl.push("object");
          sl.add("index", objIndex);
          sgmtFileIO.read_string(objPath);
          sl.add("object_path", objPath);

          uint32_t rawDataIndex{ 0 };
          sgmtFileIO.read_value(rawDataIndex);
          sl.add("raw_data_index", rawDataIndex);

          if (0xFFFFFFFF == rawDataIndex) {
            // no raw data
          }
          else if (0x0 == rawDataIndex) {
            // raw setting of last segment
            const auto previousObjRawInfo = objectRawInfosAll.find(objPath);
            if (objectRawInfosAll.end() == previousObjRawInfo) {
              throw std::logic_error("There is no raw info for this channel in the previous segment");
            }
            objectRawInfosCurr[objPath] = previousObjRawInfo->second;
          }
          else if (0x14 == rawDataIndex || 0x1c == rawDataIndex) {
            // normal raw data
            sl.push("raw");
            uint32_t rawDataType{ 0 };
            sgmtFileIO.read_value(rawDataType);
            const tdmsDataType rawDatatypeEnum = (tdmsDataType)rawDataType;
            sl.add("data_type", rawDataType);
            sl.add("data_type_string", get_tdms_data_type_as_string(rawDatatypeEnum));

            uint32_t rawDataArrayDimension{ 0 };
            sgmtFileIO.read_value(rawDataArrayDimension);
            sl.add("array_dimension", rawDataArrayDimension);

            uint64_t rawDataNumberOfValues{ 0LL };
            sgmtFileIO.read_value(rawDataNumberOfValues);
            sl.add("number_of_values", rawDataNumberOfValues);

            uint64_t totalSizeInByte{ 0LL };
            if (0x1c == rawDataIndex) {
              // only for strings
              sgmtFileIO.read_value(totalSizeInByte);
              sl.add("total_size_in_byte", totalSizeInByte);
            }

            // rember the raw element definition
            SgmtObjectRawInfo sgmtObjectRawInfo(objPath, rawDatatypeEnum, rawDataArrayDimension, rawDataNumberOfValues, totalSizeInByte);
            objectRawInfosCurr[objPath] = sgmtObjectRawInfo;
            objectRawInfosAll[objPath] = sgmtObjectRawInfo;

            sl.pop();
          }
          else if (rawDataIndex == 0x1269 || rawDataIndex == 0x1369) {
            sl.push("daqmx");
            switch (rawDataIndex) {
            case 0x1269:
              sl.add("type", "raw data contains DAQmx Format Changing scaler");
              break;
            case 0x1369:
              sl.add("type", "raw data contains DAQmx Digital Line scaler");
              break;
            }

            uint32_t rawDataType{ 0 };
            sgmtFileIO.read_value(rawDataType);
            const tdmsDataType rawDataTypeEnum = (tdmsDataType)rawDataType;
            sl.add("data_type", rawDataType);
            sl.add("data_type_string", get_tdms_data_type_as_string(rawDataTypeEnum));

            uint32_t daqmxArrayDimension{ 0 };
            sgmtFileIO.read_value(daqmxArrayDimension);
            sl.add("array_dimension", daqmxArrayDimension);

            uint64_t daqmxChunkSize { 0LL };
            sgmtFileIO.read_value(daqmxChunkSize); // Number of values
            sl.add("chunk_size", daqmxChunkSize);

            uint32_t daqmxFormatChangingScalersSize{ 0 };
            sgmtFileIO.read_value(daqmxFormatChangingScalersSize);
            sl.add("format_changing_scalers_size", daqmxFormatChangingScalersSize);
            sl.push("format_changing_scalers");
            for (uint32_t daqmxFormatChangingScalersIndex = 0UL; daqmxFormatChangingScalersIndex < daqmxFormatChangingScalersSize; ++daqmxFormatChangingScalersIndex) {
              sl.push("format_changing_scaler");
              uint32_t daqmxDataType{ 0 };
              sgmtFileIO.read_value(daqmxDataType);
              const tdmsDataType daqmxDataTypeEnum = (tdmsDataType)daqmxDataType;
              sl.add("data_type", daqmxDataType);
              sl.add("data_type_string", get_tdms_data_type_as_string(daqmxDataTypeEnum));

              uint32_t daqmxRawBufferIndex{ 0 };
              sgmtFileIO.read_value(daqmxRawBufferIndex);
              sl.add("buffer_index", daqmxRawBufferIndex);

              uint32_t daqmxRawByteOffsetWithinTheStride{ 0 };
              sgmtFileIO.read_value(daqmxRawByteOffsetWithinTheStride);
              sl.add("byte_offset_within_the_stride", daqmxRawByteOffsetWithinTheStride);

              uint32_t daqmxSampleFormatBitmap{ 0 };
              sgmtFileIO.read_value(daqmxSampleFormatBitmap);
              sl.add("sample_format_bitmap", daqmxSampleFormatBitmap);

              uint32_t daqmxScaleID{ 0 };
              sgmtFileIO.read_value(daqmxScaleID);
              sl.add("scale_id", daqmxScaleID);
              sl.pop();
            }
            sl.pop();

            uint32_t daqmxRawDataWithSize{ 0 };
            sgmtFileIO.read_value(daqmxRawDataWithSize);
            sl.add("data_with_size_vector_size", daqmxRawDataWithSize);
            sl.push("data_with_size_vector");
            for (uint32_t daqmxRawDataWithIndex = 0UL; daqmxRawDataWithIndex < daqmxRawDataWithSize; ++daqmxRawDataWithIndex) {
              uint32_t daqmxElementsInTheVector{ 0 };
              sgmtFileIO.read_value(daqmxElementsInTheVector);
              sl.add("size", daqmxElementsInTheVector);
            }
            sl.pop();
            sl.pop();
          }
          else {
            throw std::logic_error("mode not supported: unknown");
          }

          uint32_t numberOfProperties{ 0 };
          sgmtFileIO.read_value(numberOfProperties);
          sl.add("properties_count", numberOfProperties);
          sl.push("properties");
          std::string propName;
          for (uint32_t propIndex = 0; propIndex < numberOfProperties; ++propIndex) {
            sl.push("property");
            sgmtFileIO.read_string(propName);
            sl.add("name", propName);

            uint32_t propDataType{ 0 };
            sgmtFileIO.read_value(propDataType);
            tdmsDataType propDatatypeEnum = (tdmsDataType)propDataType;

            sl.add("data_type", propDataType);
            sl.add("data_type_string", get_tdms_data_type_as_string(propDatatypeEnum));

            switch (propDatatypeEnum) {
            case tdmsTypeVoid: {
              throw std::logic_error("property can not be void");
            }break;
            case tdmsTypeI8: {
              int8_t propVal{ 0 };
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeI16: {
              int16_t propVal{ 0 };
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeI32: {
              int32_t propVal{ 0 };
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeI64: {
              int64_t propVal{ 0 };
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeU8: {
              uint8_t propVal{ 0 };
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeU16: {
              uint16_t propVal{ 0 };
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeU32: {
              uint32_t propVal{ 0 };
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeU64: {
              uint64_t propVal{ 0 };
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeSingleFloat: {
              float propVal{ 0 };
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeDoubleFloat: {
              double propVal{ 0 };
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeExtendedFloat: {
              float80_ propVal;
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeSingleFloatWithUnit: {
              throw std::logic_error("with unit not allowed for property");
              return;
            }break;
            case tdmsTypeDoubleFloatWithUnit: {
              throw std::logic_error("with unit not allowed for property");
            }break;
            case tdmsTypeExtendedFloatWithUnit: {
              throw std::logic_error("with unit not allowed for property");
            }break;
            case tdmsTypeString: {
              std::string propVal;
              sgmtFileIO.read_string(propVal);
              sl.add("value", propVal);
            }break;
            case tdmsTypeBoolean: {
              uint8_t propVal;
              sgmtFileIO.read_value(propVal);
              bool boolVal{ propVal != 0 };
              sl.add("value", propVal);
            }break;
            case tdmsTypeTimeStamp: {
              int64_t propValSec{ 0LL };
              sgmtFileIO.read_value(propValSec);
              uint64_t propValFrac{ 0LL };
              sgmtFileIO.read_value(propValFrac);
              sl.push("value");
              sl.add("seconds", propValSec);
              sl.add("fraction", propValFrac);
              sl.pop();
            }break;
            case tdmsTypeFixedPoint: {
              fixpoint128_ propVal;
              sgmtFileIO.read_value(propVal);
              sl.add("value", propVal);
              return;
            }break;
            case tdmsTypeComplexSingleFloat: {
              float propVal[2]{ 0., 0. };
              sgmtFileIO.read_value(propVal[0]);
              sgmtFileIO.read_value(propVal[1]);
              sl.push("value");
              sl.add("real", propVal[0]);
              sl.add("imaginary", propVal[1]);
              sl.pop();
            }break;
            case tdmsTypeComplexDoubleFloat: {
              double propVal[2]{ 0., 0. };
              sgmtFileIO.read_value(propVal[0]);
              sgmtFileIO.read_value(propVal[1]);
              sl.push("value");
              sl.add("real", propVal[0]);
              sl.add("imaginary", propVal[1]);
              sl.pop();
            }break;
            case tdmsTypeDAQmxRawData: {
              throw std::logic_error("property can not be daqmx");
            }break;
            default:
              throw std::logic_error("unknown enum datatype found");
              break;
            }
            sl.pop();
          }
          sl.pop();

          sl.pop();
        }
        sl.pop();
      }

      // in TDMS dll the calculation is only done for NewObjList
      if (/*sgmtHeader.toc.NewObjList &&*/ objectRawInfosCurr.size() > 0) {
        // determine number of chunks

        uint64_t raw_data_size_of_one_chunk{ 0LL };

        for (const auto& sgmtRawInfo : objectRawInfosCurr) {
          // 1. Calculate the raw data size of a channel.Each channel has a Data type, 
          //    Array dimension and Number of values in meta information.
          //    Refer to the Meta Data section of this article for details.
          //    Each Data type is associated with a type size.You can get the raw data size of the channel by:
          //    type size of Data type × Array dimension × Number of values.
          //    If Total size in bytes is valid, then the raw data size of the channel is this value.
          uint64_t raw_data_size_of_a_channel = 0 != sgmtRawInfo.second.total_size_in_byte_? sgmtRawInfo.second.total_size_in_byte_ :
            uint64_t(get_tdms_data_type_byte_size(sgmtRawInfo.second.datatype_)) * sgmtRawInfo.second.dimension_ * sgmtRawInfo.second.number_of_values_;

          // 2. Calculate the raw data size of one chunk by accumulating the raw data size of all channels.
          raw_data_size_of_one_chunk += raw_data_size_of_a_channel;
        }
        // 3. Calculate the raw data size of total chunks by : Next segment offset - Raw data offset.
        //    If the value of Next segment offset is - 1, the raw data size of total chunks equals the 
        //    file size minus the absolute beginning position of the raw data.
        uint64_t raw_data_size_of_total_chunks = next_segment_offset - raw_data_offset;
        // 4. Calculate the number of chunks by : Raw data size of total chunks / Raw data size of one chunk.
        uint64_t  number_of_chunks = 0 != raw_data_size_of_one_chunk ? raw_data_size_of_total_chunks / raw_data_size_of_one_chunk : 1;

        sl.push("channel_data");
          sl.add("absolut_raw_data_byte_start", raw_data_absolute_offset_in_byte);
          sl.add("absolut_raw_data_byte_end", sgmtStartOffset + next_segment_offset);
          sl.add("interleaved", sgmtHeader.toc.InterleavedData);
          sl.add("number_of_chunks", number_of_chunks);
          sl.add("channels_count", objectRawInfosCurr.size());
          sl.push("channels");
          for (const auto& sgmtRawInfo : objectRawInfosCurr) {
            sl.push("channel");
            sl.add("path", sgmtRawInfo.second.objPath_);
            sl.add("data_type", sgmtRawInfo.second.datatype_);
            sl.add("data_type_string", get_tdms_data_type_as_string(sgmtRawInfo.second.datatype_));
            sl.add("data_type_single_value_size", get_tdms_data_type_byte_size(sgmtRawInfo.second.datatype_));
            sl.add("number_of_values_in_chunk", sgmtRawInfo.second.number_of_values_);
            sl.add("number_of_values_in_segment", sgmtRawInfo.second.number_of_values_ * number_of_chunks);
            sl.pop();
          }
          sl.pop();
        sl.pop();
      }

      sl.pop();
    }
    sl.pop();

    sl.add("segments_count", sgmtIndex);
    sl.pop();
  }

}


int main(int argc, char const *argv[])
{
    if(argc < 2) {
      std::cout << "USAGE: log_tdms_file_structure TDMSFILEPATH [XMLFILEPATH]" << std::endl;
      return -1;
    }
    
    std::string tdmsFilePath = argv[1];
    std::string xmlResultFilePath = argc > 2 ? argv[2] : tdmsFilePath + ".structure.xml";
    try {
      ContentLoggerXml structLog(xmlResultFilePath);
      log_tdms_file_structure<std::string>(tdmsFilePath, structLog);
    }
    catch(const std::exception& ex) {
      std::cerr << "EXCEPTION: " << ex.what() << std::endl;
      return -2;
    }
    return 0;
}
