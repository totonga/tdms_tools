# TDMS Dump Structure

## Content

This folder contains a small tool that scans TDMS file structure and dumps it to human readable XML file.

The internal structure of a NI TDMS file is described [here](https://www.ni.com/en-us/support/documentation/supplemental/07/tdms-file-format-internal-structure.html). The utility will uses this info to scan a TDMS and dump info to a XML file including
- binary offsets (absolute, relative) of sections
- meta information found in sections
- offsets, ... describing binary channel data sections

The XML file can be used to
- identify data in the TDMS file
- validate file structure to make sure schema or data types do match expectations

## Usage

```bash
log_tdms_file_structure TDMSFILEPATH [XMLFILEPATH]
```

Example:

``` bash
tdms_dump_structure IncrementalMetaInformationExample_step6.tdms
```

will generate `IncrementalMetaInformationExample_step3.tdms.structure.xml` containing structure information.

## Design Decision

- Use pure C++ code
  - make it as portable as possible
  - avoid any dependencies
- Needs only very little XML capabilities so it writes XML
  just using native C++ code avoiding a lib to reduce dependencies. `ContentLoggerXml` can easily be rewritten if necessary.