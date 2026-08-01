find_package(Catch2 3 CONFIG REQUIRED)

# The http sources are compiled directly into the test target rather than
# linked from afv_native.  The library hides nothing behind AFV_NATIVE_API in
# these headers, so linking the shared object would work on Unix but not under
# MSVC's dllexport rules - compiling the translation units keeps the tests
# portable and independent of the export macros.
add_executable(afv_native_tests
	http/test_smoke.cpp
	http/test_curl_perform.cpp
	http/test_transfer_manager.cpp

	${CMAKE_SOURCE_DIR}/src/http/Request.cpp
	${CMAKE_SOURCE_DIR}/src/http/CurlPerform.cpp
	${CMAKE_SOURCE_DIR}/src/http/TransferManager.cpp
)

target_link_libraries(afv_native_tests PRIVATE
	Catch2::Catch2WithMain
	CURL::libcurl
	nlohmann_json::nlohmann_json
	Poco::Foundation Poco::Net
	Threads::Threads
)

target_include_directories(afv_native_tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_BINARY_DIR}/include
)

include(Catch)
catch_discover_tests(afv_native_tests)
