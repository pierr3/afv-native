find_package(Catch2 3 CONFIG REQUIRED)

# Links afv_native rather than compiling its sources in, because the APISession
# test needs the afv layer and its dto/event dependencies.  These types carry
# no AFV_NATIVE_API, so this will not link under MSVC's dllexport rules - the
# test target is Unix-only until they are exported.
add_executable(afv_native_tests
	http/test_smoke.cpp
	http/test_curl_perform.cpp
	http/test_transfer_manager.cpp
	afv/test_api_session.cpp
	util/test_callbacks.cpp
	afv/test_radio_simulation.cpp
)

target_link_libraries(afv_native_tests PRIVATE
	afv_native
	Catch2::Catch2WithMain
	CURL::libcurl
	nlohmann_json::nlohmann_json
	Poco::Foundation Poco::Net Poco::Util
	OpenSSL::SSL OpenSSL::Crypto
	Threads::Threads
)

target_include_directories(afv_native_tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_BINARY_DIR}/include
)

include(Catch)
catch_discover_tests(afv_native_tests)
