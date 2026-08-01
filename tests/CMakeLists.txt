find_package(Catch2 3 CONFIG REQUIRED)

# The http sources are compiled in directly rather than linked from
# afv_native: these types carry no AFV_NATIVE_API, so linking the shared object
# would fail under MSVC's dllexport rules.
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
