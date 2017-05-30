add_custom_target (doc)
add_custom_target (info)
add_custom_target (html)
add_custom_target (pdf)
add_dependencies (doc info html pdf)


# Find doxygen and makeinfo
find_package(Doxygen)
find_program(MAKEINFO_EXECUTABLE NAMES makeinfo)


# check required programs are available.
if (MAGEEC_WITH_DOC AND NOT MAKEINFO_EXECUTABLE)
  message(SEND_ERROR "makeinfo missing, cannot generate user or design documentation")
endif ()

if (MAGEEC_WITH_DOXYGEN AND NOT DOXYGEN_FOUND)
  message(SEND_ERROR "doxygen missing, cannot generate api documentation")
endif()


set (doc_src ${CMAKE_CURRENT_SOURCE_DIR})

# make directories for the different types of documentation
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/info)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pdf)

if (MAKEINFO_EXECUTABLE)
  # Building MAGEEC guide
  add_custom_target (building-mageec-info
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/building-mageec.texi
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/info
    COMMENT "Generating Building MAGEEC guide info." VERBATIM)
  add_custom_target (building-mageec-html
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/building-mageec.texi "--html"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
    COMMENT "Generating Building MAGEEC guide html." VERBATIM)
  add_custom_target (building-mageec-pdf
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/building-mageec.texi "--pdf"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pdf
    COMMENT "Generating Building MAGEEC guide pdf." VERBATIM)
  add_dependencies (info building-mageec-info)
  add_dependencies (html building-mageec-html)
  add_dependencies (pdf  building-mageec-pdf)

  # MAGEEC design
  add_custom_target (mageec-design-info
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-design.texi
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/info
    COMMENT "Generating MAGEEC design manual info." VERBATIM)
  add_custom_target (mageec-design-html
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-design.texi "--html"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
    COMMENT "Generating MAGEEC design manual html." VERBATIM)
  add_custom_target (mageec-design-pdf
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-design.texi "--pdf"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pdf
    COMMENT "Generating MAGEEC design manual pdf." VERBATIM)
  add_dependencies (info mageec-design-info)
  add_dependencies (html mageec-design-html)
  add_dependencies (pdf  mageec-design-pdf)

  # MAGEEC feature extractor
  add_custom_target (mageec-feature-extractor-info
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-feature-extract.texi
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/info
    COMMENT "Generating MAGEEC feature extractor manual info." VERBATIM)
  add_custom_target (mageec-feature-extractor-html
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-feature-extract.texi "--html"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
    COMMENT "Generating MAGEEC feature extractor manual html." VERBATIM)
  add_custom_target (mageec-feature-extractor-pdf
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-feature-extract.texi "--pdf"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pdf
    COMMENT "Generating MAGEEC feature extractor manual pdf." VERBATIM)
  add_dependencies (info mageec-feature-extractor-info)
  add_dependencies (html mageec-feature-extractor-html)
  add_dependencies (pdf  mageec-feature-extractor-pdf)

  # MAGEEC GCC wrapper
  add_custom_target (mageec-gcc-wrapper-info
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-gcc.texi
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/info
    COMMENT "Generating MAGEEC GCC wrapper manual info." VERBATIM)
  add_custom_target (mageec-gcc-wrapper-html
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-gcc.texi "--html"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
    COMMENT "Generating MAGEEC GCC wrapper manual html." VERBATIM)
  add_custom_target (mageec-gcc-wrapper-pdf
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-gcc.texi "--pdf"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pdf
    COMMENT "Generating MAGEEC GCC wrapper manual pdf." VERBATIM)
  add_dependencies (info mageec-gcc-wrapper-info)
  add_dependencies (html mageec-gcc-wrapper-html)
  add_dependencies (pdf  mageec-gcc-wrapper-pdf)

  # MAGEEC Interfaces
  add_custom_target (mageec-interfaces-info
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-interfaces.texi
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/info
    COMMENT "Generating MAGEEC interfaces info." VERBATIM)
  add_custom_target (mageec-interfaces-html
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-interfaces.texi "--html"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
    COMMENT "Generating MAGEEC interfaces html." VERBATIM)
  add_custom_target (mageec-interfaces-pdf
    COMMAND ${MAKEINFO_EXECUTABLE} ${doc_src}/mageec-interfaces.texi "--pdf"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pdf
    COMMENT "Generating MAGEEC interfaces pdf." VERBATIM)
  add_dependencies (info mageec-interfaces-info)
  add_dependencies (html mageec-interfaces-html)
  add_dependencies (pdf  mageec-interfaces-pdf)

endif()

if (DOXYGEN_FOUND AND MAGEEC_WITH_DOXYGEN)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen)

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
                 ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                 @ONLY)

  add_custom_target (mageec-doxygen
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen
    COMMENT "Generating MAGEEC API documentation using doxygen." VERBATIM)

  add_dependencies (doc mageec-doxygen)
endif ()


# Install documentation by copying the various directories to the install
install (DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/info    DESTINATION doc)
install (DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html    DESTINATION doc)
install (DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pdf     DESTINATION doc)
install (DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen DESTINATION doc)
