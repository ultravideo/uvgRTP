#pragma once

/* Including this header will include all the necessary headers for using uvgRTP, but
 * you can also include the headers individually instead of this header. */ 

/**
 * \defgroup CORE_API Core API
 * \brief Available for both static and shared builds. Only ABI-safe fuctions and classes.
 */

/**
 * \defgroup EXTENDED_API Extended C++ API
 * \brief Only available in static builds, allows use of C++ types like std::string in the public API.
 */

#include "media_stream.hh"  // media streamer class
#include "session.hh"       // session class
#include "context.hh"       // context class
#include "rtcp.hh"          // RTCP

#include "clock.hh"         // time related functions
#include "frame.hh"         // frame related functions
#include "util.hh"          // types
#include "version.hh"       // version
