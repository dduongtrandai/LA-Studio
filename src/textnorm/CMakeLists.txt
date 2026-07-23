if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(vietnorm VERSION 0.1.0 LANGUAGES CXX)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    include(CTest)
endif()

include(GNUInstallDirs)

add_library(vietnorm STATIC
    src/Normalizer.cpp
    src/VietnameseTextProcessor.cpp
    src/DictionaryStore.cpp
    src/EnglishTransliterator.cpp
)

add_library(vietnorm::vietnorm ALIAS vietnorm)

qt_add_resources(vietnorm vietnorm_data
    PREFIX "/vietnorm"
    FILES
        data/acronyms.csv
        data/non-vietnamese-words.csv
)

target_compile_features(vietnorm PUBLIC cxx_std_17)
target_link_libraries(vietnorm PUBLIC Qt6::Core)
target_include_directories(vietnorm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(VietNormUnitTests
        tests/main.cpp
        tests/test_normalizer.cpp
    )
    target_link_libraries(VietNormUnitTests PRIVATE vietnorm::vietnorm Qt6::Test)
    target_include_directories(VietNormUnitTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME VietNormUnitTests COMMAND VietNormUnitTests)
endif()

install(TARGETS vietnorm
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/vietnorm)
