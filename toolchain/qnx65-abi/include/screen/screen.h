/*
 * $QNXLicenseC:
 * Copyright 2016, QNX Software Systems. All Rights Reserved.
 *
 * You must obtain a written license from and pay applicable
 * license fees to QNX Software Systems before you may reproduce,
 * modify or distribute this software, or any work that includes
 * all or part of this software.   Free development licenses are
 * available for evaluation and non-commercial purposes.  For more
 * information visit http://licensing.qnx.com or email
 * licensing@qnx.com.
 *
 * This file may contain contributions from others.  Please review
 * this entire file for other proprietary rights or license notices,
 * as well as the QNX Development Suite License Guide at
 * http://licensing.qnx.com/license-guide/ for other information.
 * $
 */
/**
*   @file screen.h
*   @brief Functions for the Screen library
*
*   This file defines Screen API functions that are supported by
*   the Screen library. The Screen API facilitates the communication
*   between applications and Screen services.
*/
 /**
*   @mainpage Screen library overview
*
*   Screen library supports the API functions that facilitate the
*   communication between applications and the Screen
*   services.
*/

/**
*   @defgroup screen_contexts Contexts
*   @defgroup screen_windows Windows
*   @defgroup screen_devices Devices
*   @defgroup screen_groups Groups
*   @defgroup screen_buffers Buffers
*   @defgroup screen_pixmaps Pixmaps
*   @defgroup screen_streams Streams
*   @defgroup screen_blits Blits
*   @defgroup screen_displays Displays
*   @defgroup screen_events Events
*   @defgroup screen_debugging Debugging
*   @defgroup screen_sessions Sessions
*/

#ifndef _SCREEN_SCREEN_H_
#define _SCREEN_SCREEN_H_

#include <process.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/neutrino.h>

__BEGIN_DECLS

#include <_pack64.h>

/**
* @brief Convenience macro to facilitate comparison of Screen versions.
*/
#define _SCREEN_MAKE_VERSION(major, minor, patch)  (((major) * 10000) + ((minor) * 100) + (patch))

/** @ brief The major version number. A difference in the major version number implies that the Screen API isn't
*           backwards compatible (e.g., A Screen application that's developed using Screen version 2.0.0 can't run
*           on a target that's running Screen version 1.0.0.).
*/
#define _SCREEN_VERSION_MAJOR  4

/** @ brief The minor version number. A difference in the minor version number implies that there may be differences in the
*           Screen API, but the changes are backwards compatible (e.g., A Screen application that's developed using
*           Screen version 2.1.0 can run on a target that's running Screen version 2.0.0.). Using feature
*           preprocessor (define below) can help quering available feature.
*/
#define _SCREEN_VERSION_MINOR  0

/** @ brief The patch version number. A difference in the patch version number implies no differences in the Screen API;
*           the changes are implementation updates only(e.g., A Screen application that's developed using
*           Screen version 2.1.0 uses the same Screen API as one from Screen version 2.1.1.).
*/
#define _SCREEN_VERSION_PATCH  3

/**
* @brief The current version of Screen.
*   - Screen version 2.0.0 indicates compatibility with QNX SDP 7.0.
*   - Screen version 3.0.0 indicates compatibility with QNX SDP 7.1.
*   - Screen version 4.0.0 indicates compatibility with QNX SDP 8.0.
*/
#define _SCREEN_VERSION        _SCREEN_MAKE_VERSION(_SCREEN_VERSION_MAJOR, _SCREEN_VERSION_MINOR, _SCREEN_VERSION_PATCH)

/** @brief The feature preprocessor definition that indicates the definition of
 *         the #SCREEN_PROPERTY_WINDOW_MANAGER_ID property.
 */
#define SCREEN_PROPERTY_WINDOW_MANAGER_ID SCREEN_PROPERTY_WINDOW_MANAGER_ID

/** @brief The feature preprocessor definition that indicates the declaration of
 *         screen_register_window_manager(). This function allows the abilities for a
 *         context to register and unregister itself as the window manager of a display.
 *         For more information, see screen_register_window_manager().
 */
#define screen_register_window_manager screen_register_window_manager

/** @brief The feature preprocessor definition that indicates the definition of
 *         screen_register_event(). This function handles registering sigevents so that
 *         screen_notify() can work as intended on 7.1 or connections with @c _NTO_COF_REG_EVENTS.
 *         For more information, see screen_register_event().
 */
#define screen_register_event screen_register_event

/** @brief The feature preprocessor definition that indicates the definition of
 *         a connection type that's used to identify a MIPI DSI connection to
 *         the display. For more information, see the #SCREEN_DISPLAY_TYPE_DSI display type.
 */
#define SCREEN_DISPLAY_TYPE_DSI SCREEN_DISPLAY_TYPE_DSI

/** @brief The feature preprocessor definition that indicates the definition of a
 *         pixel format with 8-bit Y plane, 2x1-subsampled, and interleaved
 *         U and V planes. For more information, see the #SCREEN_FORMAT_NV16 format type.
 */
#define SCREEN_FORMAT_NV16 SCREEN_FORMAT_NV16

/** @brief The feature preprocessor definition that indicates the definition of
 *         a flag to suggest to Screen that your buffer should be physically
 *         contiguous and the physical address should be provided.
 *         For more information, see the #SCREEN_USAGE_PHYSICAL usage type.
 */
#define SCREEN_USAGE_PHYSICAL SCREEN_USAGE_PHYSICAL

/** @brief The feature preprocessor definition that indicates the definition of
 *         a flag to atomically release all other buffers when a new one is acquired
 *         when you call screen_acquire_buffer().
 *         For more information, see the #SCREEN_ACQUIRE_AND_RELEASE_OTHERS acquire buffer flag.
 */
#define SCREEN_ACQUIRE_AND_RELEASE_OTHERS SCREEN_ACQUIRE_AND_RELEASE_OTHERS

/** @brief The feature preprocessor definition that indicates the definition of
 *         a flag to return from the function immediately if no buffer is available
 *         when you call screen_acquire_buffer().
 *         For more information, see the #SCREEN_ACQUIRE_DONT_BLOCK acquire buffer flag.
 */
#define SCREEN_ACQUIRE_DONT_BLOCK SCREEN_ACQUIRE_DONT_BLOCK

/** @brief The feature preprocessor definition that indicates the declaration of
 *         screen_create_window_from_class(). This function allows the ability to create a window
 *         whose properties will be based on a class that you've defined in your graphics configuration file.
 *         For more information, see screen_create_window_from_class().
 */
#define screen_create_window_from_class screen_create_window_from_class

/** @brief The feature preprocessor definition that indicates the definition of
 *         the #SCREEN_PROPERTY_FD property.
 */
#define SCREEN_PROPERTY_FD SCREEN_PROPERTY_FD

/** @brief The feature preprocessor definition that indicates the definition of
 *         the #SCREEN_NOTIFY_DISPLAY notification type
 */
#define SCREEN_NOTIFY_DISPLAY SCREEN_NOTIFY_DISPLAY

/** @brief The feature preprocessor definition that indicates the definition of a
 *         pixel format with 16-bit Y plane (10-bit effective), 2x1-subsampled, and interleaved
 *         U and V planes. For more information, see the #SCREEN_FORMAT_P010 format type.
 */
#define SCREEN_FORMAT_P010 SCREEN_FORMAT_P010

/** @brief The feature preprocessor definition that indicates the definition of a
 *         pixel format with 8-bit per color component with swapped R and B channels.
 *         For more information, see the #SCREEN_FORMAT_BGRA8888 format type.
 */
#define SCREEN_FORMAT_BGRA8888 SCREEN_FORMAT_BGRA8888

/** @brief The feature preprocessor definition that indicates the definition of a
 *         pixel format with 8-bit per color component with swapped R and B channels.
 *         For more information, see the #SCREEN_FORMAT_BGRX8888 format type.
 */
#define SCREEN_FORMAT_BGRX8888 SCREEN_FORMAT_BGRX8888

/** @brief The feature preprocessor definition that indicates the definition of a four-component,
 *         32-bit unsigned integer format that supports 10 bits for each color and 2
 *         bits for alpha. For more information, see the #SCREEN_FORMAT_RGBA1010102 format type.
 */
#define SCREEN_FORMAT_RGBA1010102 SCREEN_FORMAT_RGBA1010102

/** @brief The feature preprocessor definition that indicates the definition of a four-component,
 *         32-bit unsigned integer format that supports 10 bits for each color and 2
 *         unused bits. For more information, see the #SCREEN_FORMAT_RGBX1010102 format type.
 */
#define SCREEN_FORMAT_RGBX1010102 SCREEN_FORMAT_RGBX1010102

/** @brief The feature preprocessor definition that indicates the definition of a four-component,
 *         32-bit unsigned integer format that supports 10 bits for each color and 2
 *         bits for alpha. For more information, see the #SCREEN_FORMAT_BGRA1010102 format type.
 */
#define SCREEN_FORMAT_BGRA1010102 SCREEN_FORMAT_BGRA1010102

/** @brief The feature preprocessor definition that indicates the definition of a four-component,
 *         32-bit unsigned integer format that supports 10 bits for each color and 2
 *         unused bits. For more information, see the #SCREEN_FORMAT_BGRX1010102 format type.
 */
#define SCREEN_FORMAT_BGRX1010102 SCREEN_FORMAT_BGRX1010102

/** @brief The feature preprocessor definition that indicates the definition of
 *         a flag informing Screen that the buffer will be rendered
 *         using Vulkan. For more information, see the @c SCREEN_USAGE_VULKAN of <a href="screen_8h_1Screen_Usage_Flag_Types.xml">Screen usage flag types</a>.
 */
#define SCREEN_USAGE_VULKAN SCREEN_USAGE_VULKAN

/** @brief  The feature preprocessor definition that indicates the definition of the blend mode
 *          where source pixels are blended over the destination pixels as in the
 *          SCREEN_TRANSPARENCY_SOURCE_OVER. The final destination alpha value is set to opaque
 *          on render target write.
 */
#define SCREEN_TRANSPARENCY_SOURCE_OVER_OPAQUE SCREEN_TRANSPARENCY_SOURCE_OVER_OPAQUE

/** @brief The feature preprocessor definition that indicates the availability of
 *         screen_enqueue_render_buffer(). Producers may use this function to indicate
 *         when they are finished with a render buffer rather than letting the buffer
 *         post be that indication. For more information see,
 *         screen_enqueue_render_buffer(), screen_post_window() and
 *         screen_post_stream().
 */
#define SCREEN_EXPLICIT_ENQUEUE SCREEN_EXPLICIT_ENQUEUE

/** @brief  The feature preprocessor definition that indicates the definition of the BT.2020
 *          color space as per standard. For more information see color space definitions:
 *          <a href="screen_8h_1Screen_Color_Space_Types.xml">Screen color space types</a>.
 */
#define SCREEN_COLOR_SPACE_BT2020 SCREEN_COLOR_SPACE_BT2020

/** @brief  The feature preprocessor definition that indicates the definition of the BT.2020
 *          color space of extended YUV range. For more information see color space definitions:
 *          <a href="screen_8h_1Screen_Color_Space_Types.xml">Screen color space types</a>.
 */
#define SCREEN_COLOR_SPACE_BT2020_FULL SCREEN_COLOR_SPACE_BT2020_FULL

/** @brief  Deprecated; use @c SCREEN_DISPLAY_MODE_PREFERRED instead.
 */
#define SCREEN_MODE_PREFERRED SCREEN_DISPLAY_MODE_PREFERRED


/**
*   @ingroup screen_contexts
*/
struct _screen_context;

/**
* @brief A handle to a Screen context
* @details A context provides the setting for graphics operations within Screen.
*          All Screen API objects, with the exception of events and buffers, are created within the scope of a
*          context and access to these objects is always with respect to the context associated with
*          the object. Through using a handle to a context, you can identify and gain access to the
*          certain Screen API objects (e.g., windows, groups, displays, pixmaps) to set or change
*          their properties.
*
*          For more information, see
*          <a href="group__screen__contexts_1Screen_Context_Types.xml">Screen context types</a>.
*/
typedef struct _screen_context *screen_context_t;

/**
* @ingroup screen_displays
*/
struct _screen_display;

/**
* @brief A handle to the display (or displays) associated with the Screen context
* @details A display refers to a physical device, such as a touchscreen, that
*          presents images to viewers. Use this handle to gain access to display properties
*          (e.g., brightness, DPI, video mode).
*
*          For more information, see the
*          <a href="manual/cscreen_displaying.xml">Displaying the contents of windows</a>
*          chapter of the <em>Screen Developer's Guide</em> or the
*          <a href="manual/cscreen_displays.xml">Displays</a> section of the
*          library reference.
*/
typedef struct _screen_display *screen_display_t;

/**
* @ingroup screen_windows
*/
struct _screen_window;

/**
* @brief A handle to a Screen window.
* @details A window represents a drawing surface whose content is targeted to a display. There
*          are multiple types of windows (e.g, application, child, embedded) to accommodate the
*          organization of the content for display.
*
*          For more information, see the
*          <a href="manual/cscreen_windowing.xml">Windowing</a>
*          chapter of the <em>Screen Developer's Guide</em> or the
*          <a href="manual/cscreen_displays.xml">Windows</a> section of the
*          library reference.
*/
typedef struct _screen_window *screen_window_t;

/**
*   @ingroup screen_pixmaps
*/
struct _screen_pixmap;

/**
* @brief A handle to a Screen pixmap
* @details A pixmap is an off-screen rendering target. Its contents can be made visible by
*          association with a window.
*
*          For more information, see the
*          <a href="manual/cscreen_rendering-targets.xml">Render Targets</a>
*          chapter of the <em>Screen Developer's Guide</em> or the
*          <a href="manual/cscreen_pixmaps.xml">Pixmaps</a> section of the
*          library reference.
*/
typedef struct _screen_pixmap *screen_pixmap_t;

/**
*   @ingroup screen_buffers
*/
struct _screen_buffer;

/**
* @brief A handle to a Screen buffer
* @details A buffer is an area of memory that stores pixel data. Although a buffer can be created
*          in the scope of a context, it can't be used by Screen unless it's attached to a window,
*          stream, or pixmap. Multiple buffers can be associated with a window or stream, but only
*          one buffer can be associated with a pixmap.
*
*          After attaching your buffer to a pixmap, stream, or window, you should use the newly created buffer returned
*          by @c screen_get_pixmap_property_pv(), screen_get_stream_property_pv(), or screen_get_window_property_pv().
*
*          For more information, see the
*          <a href="manual/cscreen_resource_sharing.xml">Resource Sharing</a>
*          chapter of the <em>Screen Developer's Guide</em> or the
*          <a href="manual/cscreen_buffers.xml">Buffers</a> section of the
*          library reference.
*/
typedef struct _screen_buffer *screen_buffer_t;

/**
*   @ingroup screen_events
*/
struct _screen_event;

/**
* @brief A handle to a Screen event
* @details A Screen event notifies applications of actions such as window creations,
*          properties changing values, and keyboard events. Screen API manages one event queue per
*          context. However, you can also create an event that's independent of any association
*          with a context.
*
*          For more information, see the
*          <a href="manual/cscreen_event-handling.xml">Event Handling</a> or the
*          <a href="manual/cscreen_input-events.xml">Input Events</a>
*          chapters of the <em>Screen Developer's Guide</em>, or the
*          <a href="manual/cscreen_events.xml">Events</a> section of the
*          library reference.
*/
typedef struct _screen_event *screen_event_t;

/**
*   @ingroup screen_groups
*/
struct _screen_group;

/**
* @brief A handle to a Screen group
* @details A group is used to organize and manage multiple windows in your applications.
*          Groups establish relationships between properties of the windows belonging to
*          the same group; therefore applying changes to some properties of the group has an effect
*          on all windows belonging to that group.
*
*          For more information, see the
*           <a href="manual/cscreen_windowing-groups.xml">Window Groups</a>
*          chapter of the <em>Screen Developer's Guide</em> or the
*          <a href="manual/cscreen_groups.xml">Groups</a> section of the
*          library reference.
*/
typedef struct _screen_group *screen_group_t;

/**
*   @ingroup screen_devices
*/
struct _screen_device;

/**
* @brief A handle to a Screen device
* @details A device refers to an input device (e.g., keyboard, mouse, joystick, multi-touch display).
*          For more information, see the
*          <a href="manual/cscreen_devices.xml">Devices</a> section of the
*          library reference.
*/
typedef struct _screen_device *screen_device_t;

/**
*   @ingroup screen_sessions
*/
struct _screen_session;

/**
* @brief A handle to a Screen session
* @details An input session (session) receives one specific type of input events. Unlike windows,
*          sessions don't produce any graphical content and can't be placed into groups. The only
*          grouping that could be considered for a session is the one that's been established for
*          the window that the session is associated with.
*
*          Sessions allow applications to control who gets what types of input events and how
*          these input events are delivered. For each type of input event, there is a corresponding
*          session type.
*
*          For more information, see the
*          <a href="manual/cscreen_input-sessions.xml">Sessions</a>
*          chapter of the <em>Screen Developer's Guide</em> or the
*          <a href="manual/cscreen_sessions.xml">Sessions</a> section of the
*          library reference.
*/
typedef struct _screen_session *screen_session_t;

/**
*   @ingroup screen_streams
*/
struct _screen_stream;

/**
* @brief A handle for a Screen stream
* @details A stream allows graphical content to be produced and processed by components. With
*          streams, it's not necessary to associate their content to a display. The content can
*          be consumed by components other than Screen. However, these components can still
*          use Screen to display the content.
*
*          For more information, see the
*          <a href="manual/cscreen_resource_sharing-streams.xml">Using Streams</a>
*          chapter of the <em>Screen Developer's Guide</em> or the
*          <a href="manual/cscreen_streams.xml">Streams</a> section of the
*          library reference.
*/
typedef struct _screen_stream *screen_stream_t;

/**
*   @addtogroup screen_contexts
*   @{
*/
/** @brief The types of context masks
*          These bits are intended to be combined in a single integer
*          representing combinations of desired privileges that are applied to a
*          Screen context.
*   @anonenum Screen_Context_Types Screen context types
*/
enum {
	/** A context type that allows a process to create its own windows
	*   and control some of the window properties. Applications can't modify
	*   windows that were created by other applications and can't send
	*   events outside their process space. Application contexts aren't
	*   aware of other top-level windows in the system, nor are they
	*   allowed to operate on them. Application contexts are allowed to
	*   parent other windows, even if they are created in other contexts
	*   in other processes, and are allowed to control those windows.
	*   Application contexts that have write access to /dev/screen/.inject
	*   can inject Screen events.
	*/
	SCREEN_APPLICATION_CONTEXT = 0,

	/** A context type that requests a privileged context to allow
	*   a process to modify all windows in the system when new application
	*   windows are created or destroyed. The context also receives notifications
	*   when applications create new windows, existing application
	*   windows are destroyed, or when an application tries to change
	*   certain window properties. A process must have a user or group ID with
	*   write access to /dev/screen/.winmgr, to create a context of this
	*   type successfully.
	*
	*   Creating a context with @c SCREEN_WINDOW_MANAGER_CONTEXT adds the
	*   @c SCREEN_DISPLAY_MANAGER_CONTEXT flag; with its privileges and limitations.
	*
	*/
	SCREEN_WINDOW_MANAGER_CONTEXT = (1 << 0),

	/** A context type that requests a privileged context to allow a
	*   process to send events to any application in the system. This context
	*   type doesn't receive notifications when applications create new windows,
	*   when applications destroy existing windows, or when an application
	*   attempts to change certain window properties. A process must have a
	*   user or group ID with write access to /dev/screen/.provider, to
	*   create a context of this type successfully.
	*/
	SCREEN_INPUT_PROVIDER_CONTEXT = (1 << 1),

	/** A context type that requests a privileged context to provide
	*   access to power management functionality in order to change display
	*   power modes. A process must have a user or group ID with write
	*   access to /dev/screen/.powmgr, to create a context of this type
	*   successfully.
	*/
	SCREEN_POWER_MANAGER_CONTEXT = (1 << 2),

	/** A context type that requests a privileged context to allow a
	*   process to modify all display properties in the system. A process must
	*   have an effective user ID of root to create a context of this type
	*   successfully. A process must have a user or group ID with write
	*   access to /dev/screen/.dpymgr, to create a context of this type
	*   successfully.
	*/
	SCREEN_DISPLAY_MANAGER_CONTEXT = (1 << 3),

	/** A context type that requests a privileged context to allow a
	*   process to be notified of all input sessions created and to modify
	*   specific session properties. A process must have a user or group ID with
	*   write access to /dev/screen/.inmgr, to create a context of this
	*   type successfully.
	*/
	SCREEN_INPUT_MANAGER_CONTEXT = (1 << 4),

	/** A context type that requests a privileged context to allow a
	*   process to attach buffers that it created by calling screen_create_buffer().
	*   The context will not be involved in the creation of buffers other than the ones it chooses to
	*   attach. A process must have a user or group ID with write access
	*   to /dev/screen/.attach, to create a context of this type successfully.
	*/
	SCREEN_BUFFER_PROVIDER_CONTEXT = (1 << 5),
};
/** @} */

/**
*   @addtogroup screen_windows
*   @{
*/
 /** @brief Types of windows that can be created
 *   @anonenum Screen_Window_Types Screen window types
 */
enum {
	/** The window type that's used to display the main application. Its x and
	*   y coordinates are always relative to the dimensions of the display.
	*/
	SCREEN_APPLICATION_WINDOW = 0,

	/** The window type that's commonly used to display a dialog. You must add a
	*   child window to an application's window group; otherwise, the child
	*   window is invisible. A child window's display properties are relative
	*   to the application window to which it belongs. For example, the x and y
	*   coordinates of the child window are all relative to the top left corner
	*   of the application window. This window type has its property,
	*   @c #SCREEN_PROPERTY_FLOATING, defaulted to indicate that the window
	*   is floating.
	*/
	SCREEN_CHILD_WINDOW = 1,

	/** The window type that's used to embed a window control within an object.
	*   Like the child window, the x and y coordinates of the embedded window
	*   are all relative to the top left corner of the application window.
	*   You must add an embedded window to an application's window group;
	*   otherwise, the embedded window is invisible. This window type has its
	*   property, @c #SCREEN_PROPERTY_FLOATING, defaulted to indicate that
	*   the window is non-floating.
	*/
	SCREEN_EMBEDDED_WINDOW = 2,

	/** The flag that's used to indicate that the window is responsible for consuming the content
	*   of all windows in its group. Screen performs compositing with the root window, but won't
	*   composite any window below the root window. This flag must be used in a logical OR
	*   expression with one of the Screen window types.
	*/
	SCREEN_ROOT_WINDOW = 4,
};
/** @} */

/** @brief Properties that are associated with Screen API objects
*   @details Each property description includes the following:
*            - a general description of the property
*            - an indicator of whether the property is configurable
*               - Some properties are configurable through the @c graphics.conf configuration file.
*                 At each display's and window's initialization, the value that's set at configuration
*                 applies to all displays and windows of a specified class. See
*                 <a href="manual/cscreen_section_winmgr.xml">"Configure winmgr section"</a>
*                 for more details.
*            - its default privileges
*               - Each property comes with default privileges that define what type of access the user,
*                 the manager, and the consumer has. See
*                 <a href="manual/cscreen_permissions.xml">"Permissions and Privileges"</a>
*                 for more details. The privileges are a combination of r (read), w (write),
*                 x (execute), and - (no privilege) for the user, the manager, and the consumer respectively.
*                 The API object owner can change the user privilege at any time if it chooses to do so.
*            - an indicator of whether the property sends a change event
*                - Some properties invoke a @c #SCREEN_EVENT_PROPERTY event to be sent when their values are changed.
*                  Screen directs this event to those that are impacted by the change in the property.
*                  For example, if the visibility of a window changes, Screen notifies its manager (or parent)
*                  so that it can handle any layout adjustments accordingly.
*            - a list of Screen API objects that the property applies to
*               - When one property applies to multiple API objects, its meaning may vary
*                 between each object. Variations, if any, from the general property definition are described
*                 in correspondence with the listed API objects.
*
*   @anonenum Screen_Property_Types Screen property types
*/
enum {
	/** A single integer that defines how alpha should be interpreted. The alpha
	*   mode must be of type <a href="screen_8h_1Screen_Alpha_Mode_Types.xml">Screen alpha mode types</a>.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: Yes (alpha-mode)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - pixmap
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_ALPHA_MODE = 1,

	/** A single integer or float that's used to adjust the gamma value for a given
	*   display. As an integer, this value is in fixed-point representation with
	*   8 bits of sub-pixel precision (i.e., @c #SCREEN_PROPERTY_GAMMA is the result
	*   of your display's gamma value * 256.0f + 0.5). As a float, this value represents
	*   the gamma as a real number; however, the supported range of this real number
	*   may depend on internal representation of the floating-point values. When
	*   retrieving or setting this property type, ensure that you provide sufficient storage
	*   for one integer, or one float, depending on the API being used.
	*
	*   You must be working within a privileged context of type @c #SCREEN_DISPLAY_MANAGER_CONTEXT to use this property.
	*
	*   - Configurable: Yes (gamma)
	*   - Default privileges: @code r--rw-rw- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_GAMMA = 2,

	/** A single integer or float that's used to adjust the brightness of an object.
	*   As an integer, this value must be in the range @c [-255,255]. As a float,
	*   this corresponds to the range @c [-1.0, 1.0]; however, support at the
	*   limits of this range may depend on internal representation of the floating-point
	*   values. When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer or float, depending on the API being used.
	*   - Configurable: Yes (brightness)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*     - window
	*/
	SCREEN_PROPERTY_BRIGHTNESS = 3,

	/** A single integer that indicates the number of buffers that were created
	*   or attached to the object. When retrieving this property type, ensure that you have
	*   sufficient storage for one integer. Also note that this property is local to the API object.
	*   This means that a query for this property won't trigger a flush of the
	*   command buffer despite that get functions are usually flushing execution.
	*   - Configurable: Yes (buffer-count)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_BUFFER_COUNT = 4,

	/**
	*   A pair of integers containing the width and height, in pixels, of the buffer.
	*   When retrieving or setting this property type, ensure that you provide
	*   sufficient storage for two integers.
	*   - Configurable: Yes (buffer-size)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*     - pixmap
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_BUFFER_SIZE = 5,

	/** A single integer which is a bitmask indicating which buttons are
	*   pressed; a property of an object. Note that D-pad, A, B, X, Y, Start, Select, Left, and
	*   Right are all considered buttons on gamepads. When retrieving this property type, ensure
	*   that you provide sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device
	*     - event
	*
	*     This property is only applicable to the following types:
	*       - @c #SCREEN_EVENT_GAMEPAD
	*       - @c #SCREEN_EVENT_GESTURE
	*       - @c #SCREEN_EVENT_JOG
	*       - @c #SCREEN_EVENT_JOYSTICK
	*       - @c #SCREEN_EVENT_KEYBOARD
	*       - @c #SCREEN_EVENT_MTOUCH_MOVE
	*       - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*       - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*       - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*       - @c #SCREEN_EVENT_POINTER: In the case of a pointer, @c #SCREEN_PROPERTY_BUTTONS must
	*                                   be a combination of type
	*                                   <a href="screen_8h_1Screen_Mouse_Button_Types.xml">
	*                                      Screen mouse button types</a>.
	*/
	SCREEN_PROPERTY_BUTTONS = 6,

	/** The name of a class corresponding to one that's defined in the configuration file,
	*   @c graphics.conf. The class specifies a set of window property and value pairs which
	*   is applied to the window as initial or default values. When retrieving or setting
	*   this property type, ensure that you have sufficient storage for a character buffer.
	*   Screen permits a maximum size of 63 bytes and a terminating null character (64 bytes in total)
	*   for @c #SCREEN_PROPERTY_CLASS.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw---- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_CLASS = 7,

	/** A single integer that indicates the color space of a buffer.
	*   The color space must be of type <a href="screen_8h_1Screen_Color_Space_Types.xml">Screen color space types</a>.
	*   The default value is @c #SCREEN_COLOR_SPACE_UNCORRECTED.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer (read-only)
	*     - pixmap
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_COLOR_SPACE = 8,

	/** A single integer or float in the that is used to adjust the contrast of
	*   an object. As an integer, this value must be in the range @c [-128, 127].
	*   As a float, this corresponds to the range @c [-0.5, 0.5]. However, support
	*   at the limits of this range may depend on internal representation of the
	*   floating-point values. When retrieving or setting this property type, ensure
	*   that you have sufficient storage for one integer or float, depending on
	*   the API being used.
	*   - Configurable: Yes (contrast)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*     - window
	*/
	SCREEN_PROPERTY_CONTRAST = 9,

	/** The input device, or the handle for the input device that the event came from.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event
	*     .
	*     For the following events, this property can be a single integer representing the input
	*     device or the handle for the input device that the event came from;
	*     when retrieving or setting this property type for these events, ensure that you have
	*     sufficient storage for either one integer or one @c void pointer:
	*     - @c #SCREEN_EVENT_CLOSE
	*     - @c #SCREEN_EVENT_CREATE
	*     - @c #SCREEN_EVENT_GAMEPAD
	*     - @c #SCREEN_EVENT_INPUT
	*     - @c #SCREEN_EVENT_INPUT_CONTROL
	*     - @c #SCREEN_EVENT_JOG
	*     - @c #SCREEN_EVENT_JOYSTICK
	*     - @c #SCREEN_EVENT_KEYBOARD
	*     - @c #SCREEN_EVENT_MTOUCH_MOVE
	*     - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*     - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*     - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*     - @c #SCREEN_EVENT_POINTER
	*     .
	*     For the @c #SCREEN_EVENT_PROPERTY event, this property is an object handle for the input
	*     device. When retrieving, or setting this property for this event, ensure that you have
	*     sufficient storage for one @c void pointer.
	*
	*/
	SCREEN_PROPERTY_DEVICE = 10,

	/** A display handle. When retrieving or setting this property type, ensure
	*   that you have sufficient storage for one @c void pointer. If you're setting another display
	*   and the pipeline for the display, set the display first and the pipeline later.
	*   - Configurable: Yes (display)
	*   - Default privileges: @code rw-rw---- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - device: The display that's the focus for the specified input device. A value
	*               of NULL indicates that the input device is focused on the default display.
	*     - event:
	*              - @c #SCREEN_EVENT_PROPERTY event: The property of either a device or a window,
	*                depending on the recipient object of the event.
	*              - @c #SCREEN_EVENT_DISPLAY event: The handle of the new external display that's been
	*                detected.
	*              - @c #SCREEN_EVENT_IDLE event:The handle of the display on which a window entered
	*                an idle state.
	*     - pixmap: The display that's used to set buffer properties against, in the cases
	*               where the buffers may be displayed through either sharing or reattaching to
	*               a window.
	*     - session: The display that's associated with the specified input session if the session
	*                is active. When a session is linked to a window, the session and the window
	*                have the same display.
	*     - stream: The display that's used to set buffer properties against, in the cases
	*               where the buffers may be displayed through either sharing or reattaching to
	*               a window.
	*     - window: The display on which the specified window is shown if the window is visible.
	*               A value of NULL indicates that the window is shown on the default display.
	*               Note that setting @c #SCREEN_PROPERTY_DISPLAY invalidates the pipeline.
	*/
	SCREEN_PROPERTY_DISPLAY = 11,

	/** A handle to the EGL driver.
	*   When retrieving or setting this property type, ensure that you provide
	*   sufficient storage for one @c void pointer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*/
	SCREEN_PROPERTY_EGL_HANDLE = 12,

	/** A single integer that indicates whether the window content is flipped
	*  (content that's reflected across a horizontal axis). Any non-zero integer value indicates
	*   that the content is flipped. When retrieving or setting this property type, ensure that you have sufficient
	*   storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw---- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_FLIP = 13,

	/** A single integer that indicates the pixel format of the buffer. The format
	*   must be of type <a href="screen_8h_1Screen_Pixel_Format_Types.xml">Screen pixel format types</a>.
	*   When retrieving or setting this property type, ensure that you provide
	*   sufficient storage for one integer.
	*   - Configurable: Yes (format)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*     - pixmap
	*     - stream
	*     - window: When you set a format with alpha (e.g., @c #SCREEN_FORMAT_RGBA4444),
	*               the @c #SCREEN_PROPERTY_TRANSPARENCY property is set to
	*               @c #SCREEN_TRANSPARENCY_SOURCE_OVER as a convenience. If this isn't your
	*               intention, then we recommend that you use a pixel format type that disregards
	*               the alpha channel(e.g., @c #SCREEN_FORMAT_RGBX4444).
	*/
	SCREEN_PROPERTY_FORMAT = 14,

	/** A handle to the last buffer of the window that's been posted.
	*   When retrieving this property type, ensure that you have sufficient storage for one
	*   @c void pointer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_FRONT_BUFFER = 15,

	/** A single integer in the range @c [0, 255] that indicates the global alpha value to be applied to
	*   the window. When retrieving or setting this property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: Yes (global-alpha)
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_GLOBAL_ALPHA = 16,

	/** A single integer that contains the pipeline ID.
	*
	*   It's important to understand the difference between pipeline ordering
	*   and the z-ordering of windows. Pipeline ordering refers to the
	*   hardware layering that is specific to your target. The z-ordering of
	*   windows refers to the layering Screen uses to arrange windows. Your
	*   application can specify both the pipeline and the z-order.
	*
	*   If your application assigns pipelines manually (by means of setting the
	*   @c #SCREEN_PROPERTY_PIPELINE property), Screen respects the pipeline
	*   assignment. However, your application must ensure that the z-order
	*   values make sense with regard to the pipeline order of your target
	*   hardware. Your pipeline assignment takes precedence over z-ordering
	*   operations in Screen. Set the @c #SCREEN_USAGE_OVERLAY flag in
	*   @c #SCREEN_PROPERTY_USAGE if you're manually assigning the pipeline.
	*
	*   For example, if you assign a framebuffer to the top layer in a graphics
	*   configuration on a non-composited window which doesn't have the
	*   correct z-order set, a new window (no matter its z-order) can't be
	*   blended above the framebuffer. The same constraint
	*   applies if you assign a framebuffer to the bottom layer of a graphics
	*   configuration; your application's window can't be blended below the
	*   framebuffer. In these cases, Screen punches holes in the framebuffer
	*   in order to preserve the z-order.
	*
	*   To set the pipeline on another display, you need to first set
	*   the display and then the pipeline.
	*
	*   Screen respects your pipeline assignment unless the option
	*   @c -strict-pipelines is set to 0 in your configuration file. In this
	*   case, Screen uses a composition strategy that aims to optimize
	*   for the current scene while taking memory bandwidth and available
	*   pipelines into consideration. That is, if your visible application
	*   windows can't be displayed on the pipeline to which it was assigned,
	*   Screen uses other composition paths.
	*
	*   When retrieving or setting this property type, ensure that you have sufficient
	*   storage for one integer.
	*
	*   - Configurable: Yes (pipeline)
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_PIPELINE = 17,

	/** The group that the API object is associated with.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage according to the definition of the property for the
	*   specific API object.
	*   When you retrieve this property, as a pointer, at any point in your application,
	*   Screen locally allocates resources to track this group. You must call @c screen_destroy_group()
	*   to free up these resources when you've finished using this group.
	*   Also note that Screen permits a maximum size of 63 bytes and a terminating null character
	*  (64 bytes in total) for @c #SCREEN_PROPERTY_GROUP as a name.
	*
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event:
	*       - @c #SCREEN_EVENT_IDLE: The pointer to a group of type @c #screen_group_t whose
	*                                state is now idle.
	*       - @c #SCREEN_EVENT_PROPERTY: The pointer to a group of type @c #screen_group_t whose
	*                                    property has changed.
	*       - @c #SCREEN_EVENT_CREATE: The name of the group that a window has joined. Typically
	*                                  this property is only relevant to a
	*                                  @c #SCREEN_EVENT_CREATE event for a child or embedded window.
	*                                  A @c #SCREEN_EVENT_CREATE event for a child or embedded
	*                                  window doesn't exist unless it's associated with a parent
	*                                  (window group). Therefore, only @c #SCREEN_EVENT_CREATE events
	*                                  for a child or embedded window have a group association.
	*     - pixmap: The pointer to the group that the pixmap is associated with.
	*               - Using @c screen_get_pixmap_property_cv() to retrieve this property returns
	*                 @c #SCREEN_PROPERTY_GROUP as a name. In this case, ensure that you have
	*                 sufficient storage for a character buffer.
	*               - Using @c screen_get_pixmap_property_pv() to retrieve this property returns
	*                 @c #SCREEN_PROPERTY_GROUP as a pointer. In this case, ensure that you have
	*                 sufficient storage for a structure of type @c #screen_group_t.
	*     - window: The pointer to the group that the window has created (parented). If the window
	*               hasn't created a group, then this property refers to the group that the window
	*               has joined.
	*               - Using @c screen_get_window_property_cv() to retrieve this property returns
	*                 @c #SCREEN_PROPERTY_GROUP as a name. In this case, ensure that you have
	*                 sufficient storage for a character buffer.
	*               - Using @c screen_get_window_property_pv() to retrieve this property returns
	*                 @c #SCREEN_PROPERTY_GROUP as a pointer. In this case, ensure that you have
	*                 sufficient storage for a structure of type @c #screen_group_t.
	*/
	SCREEN_PROPERTY_GROUP = 18,

	/** A single integer or float that's used to adjust the hue. As an integer,
	*   this value must be in the range @c [-128, 127]. As a float, this corresponds
	*   to the range @c [-0.5, 0.5]; however, support at the limits of this range
	*   may depend on internal representation of the floating-point values. When
	*   retrieving or setting this property type, ensure that you have sufficient
	*   storage for one integer or float, depending on the API being used.
	*   - Configurable: Yes (hue)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*     - window
	*/
	SCREEN_PROPERTY_HUE = 19,

	/** A string that can be used by window manager or parent to identify the
	*   contents of an object. When retrieving or setting this property type, ensure that you
	*   provide sufficient storage for a character buffer. Screen permits a maximum size of
	*   63 bytes and a terminating null character (64 bytes in total) for @c #SCREEN_PROPERTY_ID_STRING.
	*   - Configurable: Yes (id_string)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*     - display
	*     - group
	*     - pixmap
	*     - session
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_ID_STRING = 20,

	/** A single integer that indicates the input value associated with an event.
	*   When retrieving or setting this property type, ensure that you provide sufficient storage
	*   for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event
	*     .
	*     This property is applicable to only a @c #SCREEN_EVENT_INPUT event.
	*/
	SCREEN_PROPERTY_INPUT_VALUE = 21,

	/** A single integer that indicates whether or not the buffer contains
	*   interlaced fields instead of progressive data. When retrieving or setting this property
	*   type, ensure that you provide sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*/
	SCREEN_PROPERTY_INTERLACED = 22,

	/** A single integer that indicates the jog count associated with an event. When retrieving or
	*   setting this property type, ensure that you provide sufficient storage for one
	*   integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event
	*     .
	*     This property is applicable to only a @c #SCREEN_EVENT_JOG event.
	*/
	SCREEN_PROPERTY_JOG_COUNT = 23,

	/** A single integer that indicates the keyboard cap associated with an event.
	*   When retrieving or setting this property type, ensure that you provide sufficient storage
	*   for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event
	*     .
	*     This property is applicable to only a @c #SCREEN_EVENT_KEYBOARD event.
	*/
	SCREEN_PROPERTY_KEY_CAP = 24,

	/** A single integer that indicates the flags associated with an event.
	*   When retrieving or setting this property type, ensure that you provide sufficient storage
	*   for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only the following events:
	*              - @c #SCREEN_EVENT_GAMEPAD
	*              - @c #SCREEN_EVENT_GESTURE
	*              - @c #SCREEN_EVENT_INPUT
	*              - @c #SCREEN_EVENT_JOG
	*              - @c #SCREEN_EVENT_JOYSTICK
	*              - @c #SCREEN_EVENT_KEYBOARD
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*              - @c #SCREEN_EVENT_POINTER
	*/
	SCREEN_PROPERTY_FLAGS = 25,

	/** A single integer that indicates the modifiers associated with an object. When retrieving or
	*   setting this property type, ensure that you provide sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*   - device: From @c sys/keycodes.h, the valid modifiers for a device are:
	*                 - @c KEYMOD_SHIFT_LOCK
	*                 - @c KEYMOD_CTRL_LOCK
	*                 - @c KEYMOD_ALT_LOCK
	*                 - @c KEYMOD_ALTGR_LOCK
	*                 - @c KEYMOD_SHL3_LOCK
	*                 - @c KEYMOD_MOD6_LOCK
	*                 - @c KEYMOD_MOD7_LOCK
	*                 - @c KEYMOD_MOD8_LOCK
	*                 - @c KEYMOD_CAPS_LOCK
	*                 - @c KEYMOD_NUM_LOCK
	*                 - @c KEYMOD_SCROLL_LOCK
	*   - event: This property is applicable to only the following events:
	*            - @c #SCREEN_EVENT_GAMEPAD
	*            - @c #SCREEN_EVENT_GESTURE
	*            - @c #SCREEN_EVENT_JOG
	*            - @c #SCREEN_EVENT_JOYSTICK
	*            - @c #SCREEN_EVENT_KEYBOARD
	*            - @c #SCREEN_EVENT_MTOUCH_MOVE
	*            - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*            - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*            - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*            - @c #SCREEN_EVENT_POINTER
	*            .
	*            See @c sys/keycodes.h for valid modifiers.
	*/
	SCREEN_PROPERTY_MODIFIERS = 26,

	/** A single integer that indicates the scan code associated with an event.
	*   When retrieving or setting this property type, ensure that you provide sufficient storage
	*   for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*   - event: This property is applicable to only the following events:
	*            - @c #SCREEN_EVENT_GESTURE
	*            - @c #SCREEN_EVENT_KEYBOARD
	*/
	SCREEN_PROPERTY_SCAN = 27,

	/** A single integer that indicates the symbols associated with an event.
	*   When retrieving or setting this property type, ensure that you provide sufficient storage
	*   for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*   - event: This property is applicable to only the following events:
	*            - @c #SCREEN_EVENT_GESTURE
	*            - @c #SCREEN_EVENT_KEYBOARD
	*/
	SCREEN_PROPERTY_SYM = 28,

	/** A single integer (0 or 1) that indicates whether or not contents of the API
	*   object are mirrored (a reflection of the original content across the vertical axis).
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_MIRROR = 29,

	/** The name of a window group or event property. When retrieving or setting this property
	*   type, ensure you provide sufficient storage for a character buffer.
	*   Screen permits a maximum size of 63 bytes and a terminating null character (64 bytes in total)
	*   - Configurable: No, displays:  Yes (name)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property applicable to the following events
	*              - @c #SCREEN_EVENT_PROPERTY event: Use this name to retrieve
	*                the property that's associated with the event.
	*                See <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
	*              - @c #SCREEN_EVENT_INPUT_CONTROL event: Use this name to retrieve
	*                the type of input control that's associated with the event.
	*                See <a href="group__screen__events_1Screen_Input_Control_Events.xml">
	*                Screen input control events</a>.
	*     - group: When setting this property, note that lower case and special characters are
	*              permitted. However, the string must be unique within your system and must not
	*              start with <tt>__scrn-</tt>.
	*/
	SCREEN_PROPERTY_NAME = 30,

	/** A single integer that indicates the process id of the process responsible
	*   for creating the API object. When retrieving this property type, ensure
	*   that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - devices
	*     - displays
	*     - group
	*     - pixmaps
	*     - session
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_OWNER_PID = 31,

	/** A single integer that indicates whether or not the buffer is physically
	*   contiguous. When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*/
	SCREEN_PROPERTY_PHYSICALLY_CONTIGUOUS = 32,

	/** Up to three integers that provide the offset from the base address for each
	*   of the Y, U, V, UV, or YUV components of packed and planar YUV formats.
	*   For RGB(A) formats, the first integer is an offset from the base address where
	*   RGB(A) data actually starts.
	*   When retrieving or setting this property type, ensure that you have sufficient
	*   storage for three integers.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*/
	SCREEN_PROPERTY_PLANAR_OFFSETS = 33,

	/** A pointer that can be used by software renderers to read from and/or write to the buffer.
	*   When retrieving or setting this property, ensure that you have sufficient storage
	*   space for one @c void pointer. You must have created or attached the buffer with a usage
	*   that includes @c #SCREEN_USAGE_READ and/or @c #SCREEN_USAGE_WRITE to be able to use this
	*   property.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*/
	SCREEN_PROPERTY_POINTER = 34,

	/** A pair of integers that define position of the screen coordinates of the related API
	*   object. When retrieving or setting this property type, ensure that you have sufficient
	*   storage for two integers.
	*   - Configurable: Yes (window-position)
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - display: The x and y positions (in pixels) of the top left corner of the display. This coordinate is absolute and
	*                is not affected by the rotation of the display.
	*     - event: The x and y values for the contact point of the mtouch or pointer.
	*              This property is applicable to only the following events:
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*              - @c #SCREEN_EVENT_POINTER
	*     - session: The x and y positions of the input region. The position is expressed relative
	*                to the top left corner of the window buffer if the session is associated with
	*                a window. Otherwise, the position is in display coordinates.
	*     - window: The x and y positions of the window screen coordinates.
	*               If the flags @c #SCREEN_FLAG_SOURCE_POSITION_VALID and @c #SCREEN_FLAG_POSITION_VALID
	*               are set, then the coordinates are relative to the window buffer.
	*               If @c #SCREEN_FLAG_SOURCE_POSITION_VALID is not set, but
	*               @c #SCREEN_FLAG_POSITION_VALID is set, then the coordinates are absolute. Remember that the
	*               position of child and embedded windows are relative to the parent window. For
	*               example, if the position of the application window is {10, 10} and the position
	*               of the child window is {10, 10}, then the position of the child window on the
	*               physical display is actually {20, 20}.
	*/
	SCREEN_PROPERTY_POSITION = 35,

	/** A single integer that specifies whether or not there is protection for the
	*   buffer. Screen won't display the content of the buffer unless there's a secure link present.
	*   Operations on the buffer, such as reading from, writing to, or mapping a region of the
	*   buffer to a different address space are prohibited. Note that setting protection on a
	*   buffer doesn't invoke a request for authentication. Typically, the window that owns
	*   the buffer will have its window property, @c #SCREEN_PROPERTY_PROTECTION_ENABLE, set.
	*   The request for authentication is made when the you post the window and its
	*   @c #SCREEN_PROPERTY_VISIBLE property indicates that the window is visible.
	*   When retrieving or setting this property type, ensure that you have sufficient storage for
	*   one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*/
	SCREEN_PROPERTY_PROTECTED = 36,

	/** A handle to the buffer or buffers available for rendering. When retrieving
	*   this property type, ensure that you provide sufficient storage according
	*   to the API object type.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - pixmap: Only one buffer is allowed for a pixmap object. When retrieving
	*               this property for a pixmap, ensure that you have sufficient storage for one
	*               @c void pointer.
	*     - stream: Multiple buffers may be available for rendering for a stream object.
	*               When retrieving @c #SCREEN_PROPERTY_RENDER_BUFFERS
	*               for a stream, ensure that you have sufficient storage for one @c void pointer for
	*               each available buffer. Use the @c #SCREEN_PROPERTY_RENDER_BUFFER_COUNT property
	*               to determine the number of buffers that are available for rendering.
	*     - window: Multiple buffers may be available for rendering for a window object.
	*               When retrieving @c #SCREEN_PROPERTY_RENDER_BUFFERS
	*               for a window, ensure that you have sufficient storage for one @c void pointer for
	*               each available buffer. Use the @c #SCREEN_PROPERTY_RENDER_BUFFER_COUNT property
	*               to determine the number of buffers that are available for rendering.
	*/
	SCREEN_PROPERTY_RENDER_BUFFERS = 37,

	/** A single integer that defines the desired rotation of the API object.
	*   The default rotation is @c #SCREEN_ROTATION_NONE.
	*   When retrieving or setting this property type, ensure that you provide
	*   sufficient storage for one integer.
	*   - Configurable: Yes (rotation (window or display)
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - display: The desired rotation, in degrees clockwise, of the display. The rotation value must be of type
	*                <a href="screen_8h_1Screen_Rotation_Types.xml">Screen rotation types</a>.
	*                Changing the display rotation doesn't implicitly change any window properties.
	*     - window: The desired rotation, in degrees clockwise, of the window's content. Window rotation doesn't
	*               affect the the size or position of the window; only the content of the window is rotated. Screen applies
	*               scaling on the content to fit the bounds of the window where applicable. The rotation value
	*               must be of type <a href="screen_8h_1Screen_Rotation_Types.xml">Screen rotation types</a>.
	*/
	SCREEN_PROPERTY_ROTATION = 38,

	/** A single integer or float that is used to adjust the saturation. As an
	*   integer, this value must be in the range @c [-128, 127]. As a float, this
	*   corresponds to the range @c [-0.5, 0.5]; however, support at the limits
	*   of this range may depend on internal representation of the floating-point
	*   values. When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer or float, depending on the API being used.
	*   - Configurable: Yes (saturation)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - displays
	*     - window
	*/
	SCREEN_PROPERTY_SATURATION = 39,

	/** The size of the associated API object. When retrieving or setting this property type,
	*   ensure that you provide sufficient storage according to the API object.
	*   - Configurable: Yes (window-size)
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer: A single integer that indicates the size, in bytes, of the buffer.
	*               When retrieving or setting this property type, ensure that you have
	*               sufficient storage for one integer.
	*     - device: A pair of integers that specify the values of a logical minimum and
	*               maximum displacement for gamepad, jog, and joystick devices. If your device
	*               supports multiple degrees of freedom, Screen applies the values of this property
	*               to all axes. When you change this property, you affect the
	*               @c #SCREEN_PROPERTY_DISPLACEMENT property for the devices. Screen uses the
	*               larger of the two integers that you specify as the logical maximum, and the
	*               smaller of the two integers as the logical minimum. Change this property of a
	*               gamepad, jog, or joystick device to control its sensitivity. Screen
	*               defines the rest position of a device (i.e., no displacement) as zero;
	*               zero is used as the midpoint between the logical maximum and minimum. This
	*               property is applicable to only the following devices:
	*               - @c #SCREEN_EVENT_GAMEPAD
	*               - @c #SCREEN_EVENT_JOG
	*               - @c #SCREEN_EVENT_JOYSTICK
	*     - display: A pair of integers that define the width and height, in pixels,
	*                of the current video resolution. When retrieving this property
	*                type, ensure that you have sufficient storage for two integers.
	*                Note that the display size changes with the display rotation.
	*                For example, if the video mode is 1024x768 and the rotation is
	*                0 degrees, the display size will indicate 1024x768. When the
	*                display rotation is set to 90 degrees, the display size will
	*                become 768x1024. This property can be only retrieved, and not set for
	*                a display.
	*     - event: A pair of integers that define the width and height, in pixels,
	*              of the touch or contact area. This property is applicable to only the
	*              following events:
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*     - session: A pair of integers that define the width and height, in pixels,
	*                of the input region. The dimensions are expressed in buffer
	*                coordinates if the session is associated with a window.
	*                Otherwise, the size is in display coordinates. When retrieving
	*                or setting this property type, ensure that you have sufficient
	*                storage for two integers.
	*     - window: A pair of integers that define the width and height, in pixels,
	*               of the window. When retrieving this property type, ensure that
	*               you have sufficient storage for two integers.
	*/
	SCREEN_PROPERTY_SIZE = 40,

	/** A pair of integers that define the x and y position of a source viewport
	*   within the window buffers. When retrieving or setting this property type,
	*   ensure that you have sufficient storage for two integers.
	*   - Configurable: Yes (source-position)
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only the following events:
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*              - @c #SCREEN_EVENT_POINTER
	*     - window: The x and y coordinates of the top left corner of a rectangular
	*               region within the window buffer representing the source viewport
	*               of the window. This is the portion of the window buffer that is to
	*               be displayed. If the flag, @c SCREEN_FLAG_SOURCE_POSITION_VALID, is set, then
	*               the coordinates are relative to the window buffer. If it's not set,
	*               then the coordinates are absolute.
	*/
	SCREEN_PROPERTY_SOURCE_POSITION = 41,

	/** A pair of integers that define the width and height, pixels, of a source
	*   viewport within the window buffers. When retrieving or setting this
	*   property type, ensure that you have sufficient storage for two integers.
	*   - Configurable: Yes (source-size)
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only the following events:
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*     - window: The width and height of a rectangular region within the window
	*               buffer representing the source viewport of the window.
	*               This is the portion of the window buffer that is to be
	*               displayed.
	*/
	SCREEN_PROPERTY_SOURCE_SIZE = 42,

	/** A single integer that indicates whether or not the contents of a window
	*   are expected to change. When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: Yes (static)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_STATIC = 43,

	/** A single integer that indicates the number of bytes between the same
	*   pixels on adjacent rows. When retrieving or setting this property type, ensure that you
	*   provide sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*/
	SCREEN_PROPERTY_STRIDE = 44,

	/** A single integer that specifies the minimum number of vsync periods
	*   between posts. When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: Yes (interval)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - streams
	*     - window
	*/
	SCREEN_PROPERTY_SWAP_INTERVAL = 45,

	/** A single integer that defines the transparency type of an API object.
	*   When retrieving or setting this property type, ensure that you
	*   have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display: How multiple layers are combined. The transparencies that are
	*                applicable to a display object are:
	*                - @c #SCREEN_TRANSPARENCY_SOURCE_COLOR
	*                - @c #SCREEN_TRANSPARENCY_SOURCE_OVER
	*                - @c #SCREEN_TRANSPARENCY_SOURCE_OVER_OPAQUE
	*                .
	*                This property can be only retrieved, and not set for a display.
	*     - window: How the alpha channel of the window is used to combine a window
	*               with other windows or the background color underneath it. Although the
	*               window transparency property can be set, the actual transparency applied
	*               is dependent on hardware. If the hardware supports it, the transparency
	*               specified by this property is applied, otherwise a best effort
	*               algorithm is used to apply the window transparency. Transparency
	*               must be of the type  <a href="screen_8h_1Screen_Transparency_Types.xml">Screen transparency types</a>.
	*               This property is set to @c #SCREEN_TRANSPARENCY_SOURCE_OVER as a convenience when
	*               you set @c #SCREEN_PROPERTY_FORMAT to a format with alpha
	*              (e.g., @c #SCREEN_FORMAT_RGBA4444). Therefore, if this isn't
	*               your intention, then we recommend that you use a pixel format
	*               type that disregards the alpha channel (e.g. @c #SCREEN_FORMAT_RGBX4444).
	*/
	SCREEN_PROPERTY_TRANSPARENCY = 46,

	/** A single integer that indicates the type of the specified object.
	*   When retrieving this property type, ensure that you provide sufficient
	*   storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*      - context: The type of context. Valid context types must be a combination of the type
	*                <a href="group__screen__contexts_1Screen_Context_Types.xml">Screen context types</a>.
	*      - device: The type of input device. Valid input device types are:
	*                - @c #SCREEN_EVENT_GAMEPAD
	*                - @c #SCREEN_EVENT_JOG
	*                - @c #SCREEN_EVENT_JOYSTICK
	*                - @c #SCREEN_EVENT_KEYBOARD
	*                - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*                - @c #SCREEN_EVENT_POINTER
	*      - display: The type of display port. Valid display ports must be of type
	*                 <a href="screen_8h_1Screen_Display_Types.xml">Screen display types</a>.
	*      - event: The type of event. Valid event types must be of type
	*               <a href="group__screen__events_1Screen_Event_Types.xml"> Screen event types</a>.
	*               This property can be retrieved or set for an event object.
	*      - session: The type of input session. Valid session types must be of type:
	*                 - @c #SCREEN_EVENT_GAMEPAD
	*                 - @c #SCREEN_EVENT_GESTURE
	*                 - @c #SCREEN_EVENT_JOG
	*                 - @c #SCREEN_EVENT_JOYSTICK
	*                 - @c #SCREEN_EVENT_KEYBOARD
	*                 - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*                 - @c #SCREEN_EVENT_POINTER
	*      - window: The type of window. Valid window types must be of type
	*                <a href="group__screen__windows_1Screen_Window_Types.xml">Screen window types</a>.
	*/
	SCREEN_PROPERTY_TYPE = 47,

	/** A single integer that is a bitmask indicating the intended usage
	*   for the buffers associated with the API object. The default usage for a
	*   buffer is @c #SCREEN_USAGE_READ | @c #SCREEN_USAGE_WRITE.
	*   @c #SCREEN_PROPERTY_USAGE must be a combination of type
	*   <a href="screen_8h_1Screen_Usage_Flag_Types.xml"> Screen usage flag types</a>.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer. Note that changing
	*   @c #SCREEN_PROPERTY_USAGE affects the pipeline when
	*   the overlay usage bit (@c #SCREEN_USAGE_OVERLAY) is added or
	*   removed.
	*   - Configurable: Yes (usage)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - pixmap
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_USAGE = 48,

	/** An array of four integers containing data associated with the user. This property
	*   can be queried or set in association with an event of type
	*   @c #SCREEN_EVENT_USER. When retrieving or setting this property
	*   type, ensure that you have sufficient storage for four integers.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event
	*/
	SCREEN_PROPERTY_USER_DATA = 49,

	/** A handle to associate the API object with application-specific user data.
	*   When retrieving or setting this property type, ensure that you have
	*    sufficient storage for one @c void pointer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device
	*     - group
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_USER_HANDLE = 50,

	/** A single integer that specifies whether an API object is visible (or
	*   active). Any non-zero integer value indicates that the API object is visible.
	*   When retrieving or setting this property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: Yes (visible)
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - session
	*     - window
	*/
	SCREEN_PROPERTY_VISIBLE = 51,

	/** A pointer to a window. When retrieving or setting this property type,
	*   ensure that you have sufficient storage for one @c void pointer.
	*   When you retrieve this property at any point in your application,
	*   Screen locally allocates resources to track this window. You must call @c screen_destroy_window()
	*   to free up these resources when you've finished using this window.
	*
	*   - Configurable: No
	*   - Default privileges: @code rw-rw---- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: The window on which the input device is focused. All input from
	*               the device is directed to this particular window.
	*     - event: This property can be only retrieved and not set for an event object.
	*              For the following events, this property refers to the window associated with
	*              the event:
	*              - @c #SCREEN_EVENT_CLOSE
	*              - @c #SCREEN_EVENT_CREATE
	*              - @c #SCREEN_EVENT_POST
	*              - @c #SCREEN_EVENT_UNREALIZE
	*              .
	*              For the following events, this property refers to the window
	*              associated with the input device for which the event is intended:
	*              - @c #SCREEN_EVENT_GAMEPAD
	*              - @c #SCREEN_EVENT_JOG
	*              - @c #SCREEN_EVENT_JOYSTICK
	*              - @c #SCREEN_EVENT_KEYBOARD
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*              - @c #SCREEN_EVENT_POINTER
	*              .
	*              For the following event, this property refers to the window whose property
	*              is being set:
	*              - @c #SCREEN_EVENT_PROPERTY
	*     - session: The window on which the input session is focused.
	*/
	SCREEN_PROPERTY_WINDOW = 52,

	/** A single integer that indicates he number of render buffers associated
	*   with the API object. When retrieving this property type, ensure that you have sufficient
	*   storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_RENDER_BUFFER_COUNT = 53,

	/** A single integer in the range @c [INT_MIN, INT_MAX] that indicates the distance (from the bottom) that's
	*   used when ordering API objects amongst each other.  When retrieving or
	*   setting this property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: Yes (zorder)
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - session: A window can have multiple input sessions. When regions of these
	*                multiple sessions overlap, this property is used to determine which
	*                session is on top. The session whose @c #SCREEN_PROPERTY_ZORDER is
	*                the highest is the session that receives the input event.
	*     - window:  The z-order of a window is relative to its parent window. For example, a positive
	*                value will place the child on top of (or above) its associated parent window. Conversely,
	*                a negative z-order puts the child window underneath the parent window. The z-order has no
	*                units associated with it. In the case that there's a window manager in the system,
	*                the z-order of application windows are relative to 0.
	*/
	SCREEN_PROPERTY_ZORDER = 54,

	/** A single @c long @c long integer that corresponds to the physical address of the
	*   buffer. This property is valid only when the buffer is physically contiguous. When
	*   retrieving or setting this property type, ensure that you provide sufficient storage for
	*   one long integer.
	*   - Configurable: No
	*   - Default privileges: @code r--rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*/
	SCREEN_PROPERTY_PHYSICAL_ADDRESS = 55,

	/** A single integer that indicates the amount of filtering performed by
	*   Screen when scaling is required to draw the window.
	*   The scale quality must be of type
	*   <a href="screen_8h_1Screen_Scale_Quality_Types.xml">Screen scale quality types</a>.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_SCALE_QUALITY = 56,

	/** A single integer that indicates the API object input behavior. The
	*   sensitivity must be of type
	*   <a href="screen_8h_1Screen_Sensitivity_Types.xml">Screen sensitivity types</a>
	*   or an integer that is a bitmask combination of
	*   <a href="screen_8h_1Screen_Sensitivity_Masks.xml">Screen sensitivity masks</a>.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: Yes (sensitivity)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - session
	*     - window
	*/
	SCREEN_PROPERTY_SENSITIVITY = 57,

	/** A single integer that specifies the scaling mode.
	*   The scale mode must be of type
	*   <a href="screen_8h_1Screen_Scale_Types.xml">Screen scale types</a>.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_SCALE_MODE = 58,

	/** A single integer containing the number of displays associated with this
	*   context. When retrieving this property type, ensure that you have sufficient storage for
	*   one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_DISPLAY_COUNT = 59,

	/** An array of display pointers. When retrieving this property type, ensure that you have
	*   sufficient storage for one @c void pointer for each display. Retrieve the
	*   @c #SCREEN_PROPERTY_DISPLAY_COUNT property to determine the number of displays
	*   associated with this context. Once you know the number of displays, you can allocate
	*   sufficient storage to retrieve @c #SCREEN_PROPERTY_DISPLAYS.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_DISPLAYS = 60,

	/** A single integer that indicates what the window's content mode is. The content mode must be of type
	*   <a href="screen_8h_1Screen_CBABC_Mode_Types.xml">Screen content mode types</a>.
	*   When getting or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: Yes (cbabc)
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_CBABC_MODE = 61,

	/** A single integer that indicates whether or not the API object is floating. Any non-zero integer
	*   value indicates that the API object is floating. When retrieving or setting this
	*   property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - device: This property is applicable to a @c #SCREEN_EVENT_POINTER device only. When a pointer device
	*               is floating, you can change positions of the icon that's representing your cursor.
	*     - window: This property can be set only on windows of type @c #SCREEN_APPLICATION_WINDOW.
	*               An application window that's floating behaves similarly to that of a window of
	*               type @c #SCREEN_CHILD_WINDOW. An application window that's non-floating is similar
	*               to window of type @c #SCREEN_EMBEDDED_WINDOW whose data is clipped to the boundaries of its
	*               parent window.  In addition, this property is considered only if the application window is
	*               in a window group where it has a parent.
	*/
	SCREEN_PROPERTY_FLOATING = 63,

	/** A single integer that indicates whether or not the display is currently attached.
	*   Any non-zero integer value indicates that the display is attached. When retrieving
	*   or setting this property type, ensure you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display: Indicates whether or not the display is connected. This property is used mostly for displays
	*                that are detachable. For displays that are not detachable (have a value of 0 for its
	*                @c #SCREEN_PROPERTY_DETACHABLE property), their @c #SCREEN_PROPERTY_ATTACHED property is always
	*                non-zero--even when the display is physically disconnected.
	*     - event:
	*              - @c #SCREEN_EVENT_DISPLAY: Indicates that a display has changed its state;
	*                                          the display has either connected or disconnected.
	*/
	SCREEN_PROPERTY_ATTACHED = 64,

	/** A single integer that indicates whether or not the display can be detached.
	*   Any non-zero integer value indicates that the display is detachable. If a display is
	*   detachable, Screen sends events of type @c #SCREEN_EVENT_DISPLAY to privileged contexts
	*   of type @c #SCREEN_DISPLAY_MANAGER_CONTEXT upon connections and disconnections of the display.
	*   For displays that are not detachable, their @c #SCREEN_PROPERTY_ATTACHED property is always
	*   non-zero--even when the display is physically disconnected.
	*   this property type, ensure you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_DETACHABLE = 65,

	/** A pair of integers that define the width and height of the native video
	*   resolution. When retrieving this property type, ensure that you have sufficient storage
	*   for two integers.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_NATIVE_RESOLUTION = 66,

	/** A single integer that indicates whether or not content protection is
	*   enabled for the API object. Any non-zero integer value indicates that content protection is enabled.
	*   You require a secure link in order to have protection enabled. When retrieving or setting
	*   this property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - display: Indicates whether High-Bandwidth Digital Content Protection (HDCP)
	*                content protection is needed for the window(s)
	*                on the display. HDCP is considered enabled as long as one
	*                window on the display has its content protection enabled and its
	*                @c #SCREEN_PROPERTY_VISIBLE property indicates that the
	*                window is visible. This property is dynamic and its value depends on
	*                the @c #SCREEN_PROPERTY_PROTECTION_ENABLE property of the
	*                window(s) that are on the display. This property can be only retrieved
	*                and can't be set for a display. A value of @c 1 indicates that HDCP
	*                is enabled, and a value of @c 0 indicates that HDCP
	*                is disabled.
	*     - event:  This property is applicable only to @c #SCREEN_EVENT_DISPLAY event.
	*               It indicates that the disabling of content protection is detected. This is
	*               likely due to the loss of a secure link to the display.
	*     - stream: Indicates whether protection is to be requested before the stream
	*               is used. A value of @c 1 indicates that protection is
	*               requested; otherwise @c indicates that protection isn't required.
	*     - window: Indicates whether authentication is to be requested before the
	*               content of the window can be displayed. Authentication is requested
	*               when the window is posted and its @c #SCREEN_PROPERTY_VISIBLE
	*               property indicates that the window is visible. A value of zero
	*               indicates that authentication isn't required, otherwise authentication is requested.
	*/
	SCREEN_PROPERTY_PROTECTION_ENABLE = 67,

	/** A pair of integers that define the x and y position of a clipped source
	*   rectangular viewport within the source buffers.
	*   When retrieving or setting this property type, ensure that you
	*   have sufficient storage for two integers.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_SOURCE_CLIP_POSITION = 68,

	/** A pair of integers that define the width and height, in millimeters,
	*   of the region that's relevant to the Screen API object. When retrieving or
	*   setting this property type, ensure that you have sufficient storage for
	*   two integers.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: Applies only to devices of type @c #SCREEN_EVENT_MTOUCH_TOUCH where
	*               @c SCREEN_PROPERTY_PHYSICAL_SIZE refers to the size of the touch-sensitive area.
	*     - display: For display objects, this property refers to the size of the display area.
	*/
	SCREEN_PROPERTY_PHYSICAL_SIZE = 69,

	/** A single integer that indicates the number of formats that the display
	*   supports. When retrieving this property type, ensure that you have sufficient storage for
	*   at least one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_FORMAT_COUNT = 70,

	/** An array of integers of size @c #SCREEN_PROPERTY_FORMAT_COUNT
	*   that defines the formats supported by the display.
	*   If the display has many layers, the list is the union of
	*   all the formats supported on all layers. Formats are of type
	*   <a href="screen_8h_1Screen_Pixel_Format_Types.xml">Screen pixel format types</a>.
	*   When retrieving this property type, ensure that you have sufficient
	*   storage for one integer for each supported format.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_FORMATS = 71,

	/** A pair of integers, with initialization value as [@c SHRT_MIN, @c SHRT_MIN] and each in the range
    *   @c [0, SHRT_MAX], that define the width and height, in pixels, of a clipped source rectangular
    *   viewport within the source buffers. When retrieving or setting this property type, ensure that
    *   you have sufficient storage for two integers. Specify the width and height as [@c SHRT_MIN, @c SHRT_MIN]
    *   to disable the source clip rectangle.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_SOURCE_CLIP_SIZE = 72,

	/** A single integer that indicates the multi-touch contact id associated
	*   with the specific event. When retrieving or setting this property type,
	*   ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only the following events:
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*/
	SCREEN_PROPERTY_TOUCH_ID = 73,

	/** A pair of integers that define the x and y position of a rectangular
	*   region within the API object. When retrieving or setting this property
	*   type, ensure that you have sufficient storage for two integers.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display: The x and y coordinates of the top left corner of a rectangular
	*                region within the display that's intended to be mapped to and
	*                redrawn to the display. In order for you to access this display
	*                property, you need to be working within a privileged context.
	*                That is, a the context in which you are accessing this display
	*                property must have been created with at least the bit mask of
	*                @c #SCREEN_DISPLAY_MANAGER_CONTEXT.
	*     - window: The x and y coordinates of the top left corner of a viewport
	*               rectangle. This viewport rectangle is used to position child
	*               windows of type @c #SCREEN_EMBEDDED_WINDOW (windows that are
	*               non-floating). The destination rectangle of this child window,
	*               as defined by @c #SCREEN_PROPERTY_POSITION and
	*               @c #SCREEN_PROPERTY_SIZE, is relative to the viewport rectangle.
	*               Adjusting the position of the parent's viewport rectangle causes
	*               the embedded window's position to change even if its destination
	*               rectangle, or the destination and source rectangles of the parent,
	*               aren't changing. You typically use the viewport rectangle to
	*               achieve the effect of scrolling or panning a source whose size
	*               is larger than the size of your window buffer.
	*/
	SCREEN_PROPERTY_VIEWPORT_POSITION = 74,

	/** A pair of integers that define the width and height of a rectangular
	*   region within the API object. When retrieving or setting this property
	*   type, ensure that you have sufficient storage for two integers. A width
	*   and height of @c INT_MIN indicates that the viewport is disabled.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display: The width and height, in pixels, of a rectangular region within the
	*                display that is intended to be mapped to and redrawn to the display.
	*                In order for you to access this display property, you need to be
	*                working within a privileged context. That is, the context in which
	*                you are accessing this display property must have been created
	*                with at least the bit mask of @c #SCREEN_DISPLAY_MANAGER_CONTEXT.
	*     - window: The width and height, in pixels, of a viewport rectangle.
	*               This viewport rectangle is used to position child
	*               windows of type @c #SCREEN_EMBEDDED_WINDOW (windows that are
	*               non-floating). The destination rectangle of this child window,
	*               as defined by @c #SCREEN_PROPERTY_POSITION and
	*               @c #SCREEN_PROPERTY_SIZE, is relative to the viewport rectangle.
	*               You typically use the viewport rectangle to
	*               achieve the effect of scrolling or panning a source whose size
	*               is larger than the size of your window buffer.
	*/
	SCREEN_PROPERTY_VIEWPORT_SIZE = 75,

	/** A single integer that indicates the multi-touch orientation associated
	*   with the specific event. When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only the following events:
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*/
	SCREEN_PROPERTY_TOUCH_ORIENTATION = 76,

	/** A single integer that indicates the multi-touch pressure associated with
	*   the specific event. When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only the following events:
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*/
	SCREEN_PROPERTY_TOUCH_PRESSURE = 77,

	/** A single @c long @c long integer that indicates a timestamp associated with
	*   the API object. Screen uses the realtime clock and not the monotonic clock when calculating
	*   the timestamp. When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one @c long @c long integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: The timestamp (in milliseconds) at which the event was received by Screen
	*              This property can be retrieved or set for an event object.
	*     - stream and window: The timestamp (in nanoseconds) to indicate the start of a frame.
	*                          The application can set this timestamp to any specific time.
	*                          Note that Screen automatically calculates and sets this property to the current
	*                          time when you specify NULL for this property. For example, an application can
	*                          measure the CPU time that's elapsed between when it
	*                          starts rendering and when it posts the window or stream. In this case,
	*                          the application specifies NULL when setting the @c #SCREEN_PROPERTY_TIMESTAMP
	*                          property of the window or stream. The application, then uses the
	*                          @c screen_get_*_property_llv() function to retrieve the
	*                          @c #SCREEN_PROPERTY_METRICS property of the window or stream to look at the
	*                          timestamp for comparison to the timestamp that was previously set.
	*/
	SCREEN_PROPERTY_TIMESTAMP = 78,

	/** A single integer that is associated with the specific API object.
	*   Don't confuse this identifier with @c #SCREEN_PROPERTY_ID, which is an identifier
	*   that's used by applications to identify specific API objects.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context: The unique identifier used by Screen to track, in sequence, when
	*                a context was created.
	*     - device: The unique identifier used by Screen to track, in sequence, when
	*                a device was created.
	*     - event: The identifier (unique per device) to indicate the order that the events occurred
	*              from the device:
	*              - @c #SCREEN_EVENT_GAMEPAD
	*              - @c #SCREEN_EVENT_GESTURE
	*              - @c #SCREEN_EVENT_INPUT
	*              - @c #SCREEN_EVENT_JOG
	*              - @c #SCREEN_EVENT_JOYSTICK
	*              - @c #SCREEN_EVENT_POINTER
	*              - @c #SCREEN_EVENT_KEYBOARD
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*     - group: The unique identifier used by Screen to track, in sequence, when
	*                a group was created.
	*     - pixmap: The unique identifier used by Screen to track, in sequence, when
	*                a pixmap was created.
	*     - session: The unique identifier used by Screen to track, in sequence, when
	*                a session was created.
	*     - stream: The unique identifier used by Screen to track, in sequence, when
	*                a stream was created.
	*     - window: The unique identifier used by Screen to track, in sequence, when
	*               a window was created.
	*/
	SCREEN_PROPERTY_SEQUENCE_ID = 79,

	/** A single integer indicating the idle mode of the window. The idle mode must be of type
	*   <a href="screen_8h_1Screen_Idle_Mode_Types.xml">Screen idle mode types</a>.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_IDLE_MODE = 80,

	/** A single integer that indicates the idle state of the API object.
	*   The idle state is 1 if the system is idle. An idle system means that no input
	*   is received after the idle timeout period (@c #SCREEN_PROPERTY_IDLE_TIMEOUT).
	*   The idle state 0 if an input event is received prior to the idle timeout period expiring.
	*   When retrieving this property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: The idle state that's applicable to the entire device.
	*     - context and display: The idle state that's applicable to the entire display. In the case
	*                            of a context, the idle state is associated with the default display
	*                            only.
	*     - event: Indicates that an idle state change has taken place for either a display or
	*              group object. Query the @c #SCREEN_PROPERTY_OBJECT_TYPE
	*              property of the event to determine the object type of this event.
	*              This property is applicable to only the @c #SCREEN_EVENT_IDLE event.
	*     - group: The idle state that's applicable to only the group. A group is
	*              considered in idle state if none of the windows that are part of
	*              the group receive input after the idle timeout period for the group.
	*/
	SCREEN_PROPERTY_IDLE_STATE = 81,

	/** A single integer that indicates the number of windows with an idle mode
	*   of type @c #SCREEN_IDLE_MODE_KEEP_AWAKE that are visible on
	*   a display. When retrieving this property type, ensure that you have sufficient storage
	*   for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*     - groups: The number of windows in that group with an idle mode of type
	*               @c #SCREEN_IDLE_MODE_KEEP_AWAKE.
	*/
	SCREEN_PROPERTY_KEEP_AWAKES = 82,

	/** A single @c long @c long integer that indicates the amount of time, in nanoseconds,
	*   after which the system will enter an idle state. When retrieving or
	*   setting this property type, ensure that you have sufficient storage for
	*   one @c long @c long integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context: The amount of time after which the display of the context will
	*                enter an idle state.
	*     - device: The amount of time after which the device will enter an idle state.
	*     - display: The amount of time after which the display will enter an idle state.
	*     - group: The amount of time after which the group will enter in an idle state.
	*/
	SCREEN_PROPERTY_IDLE_TIMEOUT = 83,

	/** A handle or an indicator which corresponds to the object that currently has
	*   any type of input focus, such as keyboard, or mouse - with the exception of
	*   touch or pointer focus. When retrieving or setting this property type, ensure
	*   that you have sufficient storage according to the definition of the
	*   property for the specific API object.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - context and display: A handle to the top-level window (application window) on the
	*                            display that currently has focus. You must be
	*                            working within a privileged context of type
	*                            @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able to use this property.
	*                            When retrieving or setting this property type, ensure
	*                            that you have sufficient storage for one @c void pointer.
	*     - group: A handle to the immediate window in the group that currently has
	*              focus. You must be the owner of the group,
	*              @c #screen_group_t, to be able to set this property.
	*              When retrieving or setting this property type, ensure
	*              that you have sufficient storage for one @c void pointer.
	*     - session and window: A single integer (0 or 1) that indicates whether the session currently
	*                           has focus (e.g., 0 = doesn't have focus, and 1 = has focus). When
	*                           retrieving this property type, ensure that you have sufficient storage for
	*                           one integer.
	*/
	SCREEN_PROPERTY_FOCUS = 84,

	/** A handle or an indicator which corresponds to the API object that
	*   currently has mtouch focus. When retrieving or setting this property type, ensure
	*   that you have sufficient storage according to the definition of the
	*   property for the specific API object.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - context and display: A handle to the top-level window (application window) on the
	*                            display that currently has the mtouch focus. You must be working within a
	*                            privileged context of type @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able to use
	*                            this property. When retrieving or setting this property type, ensure
	*                            that you have sufficient storage for one @c void pointer.
	*     - group: A handle to the immediate window in the group that currently has
	*              the mtouch focus. You must be the owner of the group,
	*              @c #screen_group_t, to be able to set this property.
	*              When retrieving or setting this property type, ensure
	*              that you have sufficient storage for one @c void pointer.
	*     - session and window: A single integer (0 or 1) to indicate whether the input session
	*                           currently has the mtouch focus (e.g., 0 = doesn't have focus,
	*                           and 1 = has focus).
	*/
	SCREEN_PROPERTY_MTOUCH_FOCUS = 85,

	/** A handle or an indicator which corresponds to the API object that currently has
	*   pointer focus. When retrieving or setting this property type, ensure that
	*   you have sufficient storage according to the definition of the property for the
	*   specific API object.
	*   - Configurable: No
	*   - Default privileges: @code r--rw-r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - context and display: A handle to the top-level window (application window) on the
	*                            display that currently has the pointer focus. You must be working within a
	*                            privileged context of type @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able to use
	*                            this property. When retrieving or setting this property type, ensure
	*                            that you have sufficient storage for one @c void pointer.
	*     - group: A handle to the immediate window in the group that currently has
	*              the pointer focus. You must be the owner of the group,
	*              @c #screen_group_t, to be able to set this property.
	*              When retrieving or setting this property type, ensure
	*              that you have sufficient storage for one @c void pointer.
	*     - session and window: A single integer (0 or 1) to indicate whether the input session
	*                           currently has the pointer focus (e.g., 0 = doesn't have focus,
	*                           and 1 = has focus).
	*/
	SCREEN_PROPERTY_POINTER_FOCUS = 86,

	/** A unique integer, or string, that indicates the identification of the API object. This
	*   identifier is generated by Screen. Use this property, as a string, when setting permissions,
	*   and when child windows are joining a window hierarchy. This is the string that should be
	*   communicated to any window that wants to join a window group as a child. When retrieving this
	*   property type, ensure that you have sufficient storage for one integer, or a character buffer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer (can be retrieved as an integer only)
	*     - context
	*     - device
	*     - display
	*     - group
	*     - pixmap
	*     - session
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_ID = 87,

	/** A single integer that defines the power mode. Power modes must be of type
	*   <a href="screen_8h_1Screen_Power_Mode_Types.xml">Screen power mode types</a>.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: Yes (power-mode)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device
	*     - display
	*/
	SCREEN_PROPERTY_POWER_MODE = 88,

	/** A single integer that indicates the number of modes supported by the
	*   display. When retrieving this property type, ensure that you have sufficient storage for
	*   one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_MODE_COUNT = 89,

	/** The mode associated with the API object.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw---- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - display: A pointer to a structure of type @c #screen_display_mode_t, whose content is
	*                based on the current video mode.
	*
	*                When retrieving the current display mode, ensure that you have sufficient
	*                storage for a structure of type @c #screen_display_mode_t.
	*                Use @c screen_get_display_property_pv() to retrieve the current display mode.
	*
	*                When setting the current display mode, you must specify the index
	*                corresponding to the display mode. This index is determined from the
	*                @c index field of @c #screen_display_mode_t. Use @c screen_set_display_property_iv()
	*                to set the current display mode. When you want Screen to determine the optimal
	*                display mode, specify @c #SCREEN_DISPLAY_MODE_PREFERRED_INDEX as the index.
	*
	*                Use @c screen_get_display_modes() to retrieve the complete list of modes
	*                that are supported for a specified display.
	*     - device: The input mode for a device object. Valid modes are 0 and values of type
	*               <a href="group__screen__sessions_1Screen_Input_Mode_Types.xml">Screen input mode types</a>.
	*                A value of 0 means that the input mode isn't specified.
	*     - event: This property is applicable to only a @c #SCREEN_EVENT_DISPLAY event.
	*     - stream: The mode for a stream object. Valid modes are of type
	*               <a href="group__screen__streams_1Screen_Stream_Mode_Types.xml">Screen stream mode types</a>.
	*     - session and window: The input mode for a window object. Valid modes are 0 and values of type
	*                           <a href="group__screen__sessions_1Screen_Input_Mode_Types.xml">Screen input mode types</a>.
	*                           A value of 0 means that the input mode isn't specified.
	*/
	SCREEN_PROPERTY_MODE = 90,

	/** A pair of integers that define the x and y position of a clipped
	*   rectangular viewport within the window buffers. When retrieving or setting this property
	*   type, ensure that you have sufficient storage for two integers.
	*   - Configurable: Yes (@c clip-position)
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_CLIP_POSITION = 91,

	/** A pair of integers, each in the range @c [0, INT_MAX/256], that define the width and height,
	*   in pixels, of a clipped rectangular viewport within the window buffers.
	*   When retrieving or setting this property type, ensure that you have sufficient storage for
	*   two integers. Specify the width and height as [@c INT_MIN, @c INT_MIN] to disable the clip
	*   rectangle.
	*   - Configurable: Yes (@c clip-size)
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_CLIP_SIZE = 92,

	/** A single integer that indicates the background color of the window;
	*   The color format is alpha value in bits 24 to 31,
	*   red in bits 16 to 23, green in bits 8 to 15 and blue in bits 0 to 7.
	*   When retrieving or setting this property type, ensure that you have sufficient storage
	*   for one integer.
	*   - Configurable: Yes (@c color)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window: For framebuffer defaults, see the
	*     <a href="manual/cscreen_section_winmgr_class.xml#screen_winmgr_class/color">color</a> parameter of the
	*     "Configure winmgr section".
	*
	*/
	SCREEN_PROPERTY_COLOR = 93,

	/** A single integer that indicates the number and direction of mouse wheel
	*   ticks in a vertical direction. Wheel ticks in an upward direction are indicated by a
	*   negative (-) number, while those in the downward direction are indicated by a positive (+)
	*   one. When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only a @c #SCREEN_EVENT_POINTER event.
	*/
	SCREEN_PROPERTY_MOUSE_WHEEL = 94,

	/** A pointer to the context associated with the API object.
	*   When retrieving this property type, ensure that you have sufficient
	*   storage for a @c void pointer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device
	*     - display
	*     - event
	*     - group
	*     - pixmap
	*     - session
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_CONTEXT = 95,

	/** A single integer that enables an on-screen plot or a list of window
	*   statistics as a debugging aid. The debug type must be a bitmask that represents a
	*   combination of the types
	*   <a href="group__screen__debugging_1Screen_Debug_Graph_Types.xml">Screen debug graph types</a>.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw---- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_DEBUG = 96,

	/** A handle to an alternate screen object. When retrieving or setting
	*   this property type, ensure that you have sufficient storage for a
	*   @c void pointer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - window
	*     - session
	*/
	SCREEN_PROPERTY_ALTERNATE = 97,

	/** A single integer containing the number of devices associated
	*   with this object. When retrieving this property type, ensure that you
	*   have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context: The number of input devices associated with this context.
	*     - session: The number of input devices associated with this session.
	*/
	SCREEN_PROPERTY_DEVICE_COUNT = 98,

	/** A single integer that indicates the object type associated the with the
	*   specific event. When retrieving this property type, ensure that you have sufficient storage
	*   for one integer. Object types must be of type
	*   <a href="screen_8h_1Screen_Object_Types.xml">Screen object types</a>.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event
	*/
	SCREEN_PROPERTY_OBJECT_TYPE = 100,

	/** An array of device pointers. When retrieving this property type, ensure
	*   that you have sufficient storage for one @c void pointer for each device.
	*   Retrieve the @c #SCREEN_PROPERTY_DEVICE_COUNT property to find out
	*   how many devices are associated with this API object. Once you know
	*   the number of devices, you can allocate sufficient storage to retrieve
	*   @c #SCREEN_PROPERTY_DEVICES.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_DEVICES = 101,

	/** A single integer that indicates which page of a multi-page keymap
	*   must be used to translate scan codes into key caps and key symbols.
	*   Setting this property on a USB or Bluetooth keyboard device has no effect.
	*   The privileged context (@c #SCREEN_INPUT_PROVIDER_CONTEXT) that created the
	*   external keyboard device (by calling @c screen_create_device_type()) receives a notification
	*   of changes made to this property. When retrieving this property type, ensure that you have
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw---- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device
	*/
	SCREEN_PROPERTY_KEYMAP_PAGE = 102,

	/** A single integer that indicates whether the window has self
	*   layout capabilities. When set to true(1), the owner of the window can set
	*   properties that could otherwise only be changed by its manager. When set to false(0),
	*   the owner of the window is permitted to change only certain window properties.
	*   Note that only a window manager (parent) can set this property.
	*   The following are properties that can be set by the owner
	*   of the window only if @c #SCREEN_PROPERTY_SELF_LAYOUT is set to true(1):
	*   - @c #SCREEN_PROPERTY_CLIP_POSITION
	*   - @c #SCREEN_PROPERTY_CLIP_SIZE
	*   - @c #SCREEN_PROPERTY_DISPLAY
	*   - @c #SCREEN_PROPERTY_PIPELINE
	*   - @c #SCREEN_PROPERTY_POSITION
	*   - @c #SCREEN_PROPERTY_SIZE
	*   - @c #SCREEN_PROPERTY_VISIBLE
	*   - @c #SCREEN_PROPERTY_ZORDER
	*   .
	*   When these layout-related properties are changed, a @c #SCREEN_EVENT_PROPERTY event is
	*   sent to notify the window manager (or parent) of the property change.
	*
	*   When retrieving or setting @c #SCREEN_PROPERTY_SELF_LAYOUT, ensure that you
	*   have sufficient storage for one integer.
	*   - Configurable: Yes (@c self-layout)
	*   - Default privileges: @code r--rw-r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_SELF_LAYOUT = 103,

	/** A single integer containing the number of groups associated with this
	*   context. When retrieving this property
	*   type, ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_GROUP_COUNT = 104,

	/** An array of group pointers. When retrieving this property type, ensure that you have
	*   sufficient storage for one @c void pointer for each group. Retrieve the
	*   @c #SCREEN_PROPERTY_GROUP_COUNT property to determine the number of groups that
	*   are associated with this context. Once you know the number of groups, you can allocate
	*   sufficient storage to retrieve @c #SCREEN_PROPERTY_GROUPS.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_GROUPS = 105,

	/** A single integer containing the number of pixmaps associated with this
	*   context. When retrieving or setting this property type, ensure that you have sufficient
	*   storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_PIXMAP_COUNT = 106,

	/** An array of pixmap pointers; a property of a context object.
	*   When retrieving this property type, ensure that you have sufficient
	*   storage for one @c void pointer for each pixmap. Retrieve the
	*   @c #SCREEN_PROPERTY_PIXMAP_COUNT property to determine
	*   the number of pixmaps are associated with this context. Once you know
	*   the number of pixmaps, you can allocate sufficient storage to retrieve
	*   @c #SCREEN_PROPERTY_PIXMAPS.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_PIXMAPS = 107,

	/** A single integer containing the number of windows associated with this context.
	*   When retrieving this property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_WINDOW_COUNT = 108,

	/** An array of window pointers. When retrieving this property type, ensure that you have
	*   sufficient storage for one @c void pointer for each window. Retrieve the
	*   @c #SCREEN_PROPERTY_WINDOW_COUNT property to determine the number of windows are associated
	*   with this context. Once you know the number of windows, you can allocate sufficient storage
	*   to retrieve @c #SCREEN_PROPERTY_WINDOWS.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_WINDOWS = 109,

	/** A character string specifying a keymap filename without the @c .kbd extension.
	*   The @c .kbd extension is appended to the provided filename. You must be working from within
	*   a privileged context to be able to set this property. When retrieving or setting this
	*   property type, ensure that you provide a character buffer. Screen permits a maximum size of
	*   63 bytes and a terminating null character (64 bytes in total) for @c #SCREEN_PROPERTY_KEYMAP.
	*   - Configurable: Yes (@c keymap)
	*   - Default privileges: @code rw-rw---- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context: The default keymap. Unless specifically assigned, this keymap
	*                is applied to all input devices.
	*     - device: The keymap that's assigned to the specified input device. The
	*               keymap is applicable to only that device, and it isn't persistent.
	*               For example, if the input device is removed, and then replaced,
	*               the default keymap is applied to it until a keymap is
	*               specifically set for the input device again.
	*/
	SCREEN_PROPERTY_KEYMAP = 110,

	/** A single integer that indicates the number and direction of mouse wheel
	*   ticks in a horizontal direction. Wheel ticks toward the left are indicated by a
	*   negative (-) number, while those toward the right are indicated by a positive (+) one. When
	*   retrieving or setting this property type, ensure that you have sufficient storage for one
	*   integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only a @c #SCREEN_EVENT_POINTER event.
	*/
	SCREEN_PROPERTY_MOUSE_HORIZONTAL_WHEEL = 111,

	/** A single integer that indicates the touch type associated the with the
	*   specific event. When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one integer. Touch types must be of type
	*   <a href="screen_8h_1Screen_Touch_Types.xml"> Screen touch types</a>.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only the following events:
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*/
	SCREEN_PROPERTY_TOUCH_TYPE = 112,

	/** A pointer an opaque type. Use this for drivers only.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one @c void pointer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*/
	SCREEN_PROPERTY_NATIVE_IMAGE = 113,

	/** A single integer that indicates the number of bits of the desired
	*   sub-pixel precision. When retrieving or setting this property type,
	*   ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: The default value is @c 16. The maximum value is @c 30. Note that setting this property prior to
	*               retrieving @c #SCREEN_PROPERTY_TRANSFORM or
	*               @c #SCREEN_PROPERTY_BALLISTIC_COEFFICIENTS will affect the values for
	*               transformation and ballistic coefficients matrices respectively.
	*     - display: The default value is @c 16. The maximum value is @c 30.
	*     - session: The default value is @c 16. The maximum value is @c 30.
	*     - event: The default value is @c 0. The maximum value is @c 30.
	*     - window: The default value is @c 16. The maximum value is @c 30. Note that setting this property prior to
	*               retrieving @c #SCREEN_PROPERTY_TRANSFORM will affect the values for the
	*               transformation matrix.
	*/
	SCREEN_PROPERTY_SCALE_FACTOR = 114,

	/** A pair of integers which represent the dpi measurement;  a property of
	*   a display object. The dpi is calculated from the physical dimensions and
	*   resolution of the display. When retrieving this property type, ensure
	*   that you have sufficient storage for two integers.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r--@endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device
	*     - display
	*/
	SCREEN_PROPERTY_DPI = 115,

	/** A single integer that indicates the number of metrics associated with
	*   an API object.  Note that the metric count may vary between API objects. When retrieving
	*   this property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device
	*     - display
	*     - pixmap
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_METRIC_COUNT = 116,

	/** A array of metrics associated with an API object. Note that the size of
	*   the array of metrics may vary between API objects. When retrieving
	*   this property type, ensure that you have sufficient storage for one
	*   @c void pointer for each metric. Retrieve the
	*   @c #SCREEN_PROPERTY_METRIC_COUNT property to find out
	*   how many metrics are associated with the API object; once you know
	*   the number of metrics, you can allocate sufficient storage to retrieve
	*   @c #SCREEN_PROPERTY_METRICS.
	*   - Configurable: No
	*   - Default privileges: @code r--r--rw- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device
	*     - display
	*     - pixmap
	*     - session
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_METRICS = 117,

	/** A single integer that indicates the number of buttons on the input device.
	*   You can set this property on input devices you create. In the case of mtouch, this
	*   property refers to the number of buttons on the stylus, not necessarily the touch points.
	*   When retrieving or setting this property type, ensure that you have sufficient storage
	*   for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device
	*/
	SCREEN_PROPERTY_BUTTON_COUNT = 118,

	/** An integer or a string that can be used to identify the vendor of the specified API
	*   object. When retrieving or setting this property type as a character array, Screen permits
	*   a maximum size of 63 bytes and a terminating null character (64 bytes in total).
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: The integer variation of this property can provide information that's more
	*               specific than that of its string variation. When retrieving or setting this
	*               property type, ensure that you provide sufficient storage for a single integer
	*               or a character buffer.
	*     - display: For display objects, this property can only be retrieved as a string. When
	*                retrieving this property type, ensure that you provide a character buffer.
	*/
	SCREEN_PROPERTY_VENDOR = 119,

	/** An integer or a string that can be used to identify the product of the specified API
	*   object. When retrieving or setting this property type as a character array, Screen permits
	*   a maximum size of 63 bytes and a terminating null character (64 bytes in total).
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: The integer variation of this property can provide information that's more
	*               specific than that of its string variation. When retrieving or setting this
	*               property type, ensure that you provide sufficient storage for a single integer
	*               or a character buffer.
	*     - display: For display objects, this property can only be retrieved as a string. When
	*                retrieving this property type, ensure that you provide a character buffer.
	*/
	SCREEN_PROPERTY_PRODUCT = 120,

	/** A pair of integers that define the x and y position of a clipped
	*   rectangular area within the window buffers where brush strokes are
	*   allowed to be drawn. When retrieving or setting this property type, ensure that
	*   you have sufficient storage for two integers.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window
	*     - session: This property is only applicable to sessions of type
	*                @c #SCREEN_EVENT_MTOUCH_TOUCH, @c #SCREEN_EVENT_POINTER
	*                and @c #SCREEN_EVENT_NONE. The default is @c #SCREEN_EVENT_NONE.
	*/
	SCREEN_PROPERTY_BRUSH_CLIP_POSITION = 121,

	/** A pair of integers that define the width and height, in pixels, of a
	*   clipped rectangular area within the window buffers where brush strokes
	*   are allowed to be drawn. When retrieving or setting this property type, ensure that
	*   you have sufficient storage for two integers.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - session: This property is applicable to only sessions of type
	*                @c #SCREEN_EVENT_MTOUCH_TOUCH, @c #SCREEN_EVENT_POINTER.
	*                and @c #SCREEN_EVENT_NONE. The default is @c #SCREEN_EVENT_NONE.
	*     - window
	*/
	SCREEN_PROPERTY_BRUSH_CLIP_SIZE = 122,

	/** The x, y and z values for one analog controller. For analog controllers that don't have
	*   three degrees of freedom, only x and y values are valid; z has a value of 0.
	*   Regardless of two or three degrees of freedom of your analog controller(s), when retrieving
	*   or setting this property type, ensure that you have sufficient storage for three integers.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device:  Retrieve the @c #SCREEN_PROPERTY_ANALOG_COUNT property to first find out
	*                     how many controllers are associated with the device. This property is valid only
	*                     for @c #SCREEN_EVENT_GAMEPAD or @c #SCREEN_EVENT_JOYSTICK devices that
	*                     have at least one analog controller.
	*     - event: This property is applicable to only the following events:
	*              - @c #SCREEN_EVENT_GAMEPAD
	*              - @c #SCREEN_EVENT_JOYSTICK
	*/
	SCREEN_PROPERTY_ANALOG0 = 123,

	/** The x, y, and z values for a second analog controller. For analog controllers that don't
	*   have three degrees of freedom, only x and y values are valid; z has a value of 0.
	*   Regardless of two or three degrees of freedom of your analog controller(s), when retrieving
	*   or setting this property type, ensure that you have sufficient storage for three integers.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device:  Retrieve the @c #SCREEN_PROPERTY_ANALOG_COUNT property to first find out
	*                     how many controllers are associated with the device. This property is valid only for
	*                      @c #SCREEN_EVENT_GAMEPAD or  @c #SCREEN_EVENT_JOYSTICK devices that
	*                     have at least two analog controllers.
	*     - event: This property is applicable to only the following events:
	*              - @c #SCREEN_EVENT_GAMEPAD
	*              - @c #SCREEN_EVENT_JOYSTICK
	*/
	SCREEN_PROPERTY_ANALOG1 = 124,

	/** The pixmap that contains the brush to be used when the property,
	*   @c #SCREEN_PROPERTY_SENSITIVITY, has a brush bit set, and the
	*   corresponding type of input event is delivered to the API object.
	*   The sensitivity values that can be set to enable brush drawing in your
	*   sensitivity mask are:
	*   - @c #SCREEN_SENSITIVITY_MASK_POINTER_BRUSH for a mouse
	*   - @c #SCREEN_SENSITIVITY_MASK_FINGER_BRUSH for touch
	*   - @c #SCREEN_SENSITIVITY_MASK_STYLUS_BRUSH for a stylus
	*   .
	*   The pixmap can use an RGB color format for color information. The alpha
	*   channel can be used to define the brush shape. The pixmap buffer size
	*   determines the size of the brush. No drawing will occur if the
	*   window has a brush sensitivity bit set, but no brush pixmap.
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one @c void pointer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - session: This property is applicable to only sessions of type
	*                @c #SCREEN_EVENT_MTOUCH_TOUCH and @c #SCREEN_EVENT_POINTER.
	*     - window
	*/
	SCREEN_PROPERTY_BRUSH = 125,

	/** A single integer that indicates the index of the highest possible touch ID.
	*   This property can be queried only from an input device that supports mtouch (i.e., a device
	*   of type @c #SCREEN_EVENT_MTOUCH_TOUCH). When retrieving this property type,
	*   ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: This property is applicable to only input devices of type
	*               @c #SCREEN_EVENT_MTOUCH_TOUCH.
	*/
	SCREEN_PROPERTY_MAXIMUM_TOUCH_ID = 126,

	/** A set of integers or float that represent the 3x3 transformation matrix
	*   used to convert buffer or device coordinates to display coordinates. The
	*   transformation matrix is stored in column-major order. This property is
	*   typically used when you have a known coordinate and need to know where
	*   it would be located on the display screen. Screen creates a vector
	*   V=(X, Y, 1.0) from the raw coordinates, (X, Y). Then, Screen multiplies
	*   the transformation matrix T with this vector (T x V) to achieve a
	*   resultant vector of (Xa, Ya, Wa). This resultant vector is then divided
	*   by Wa to result in (Xab, Yab, 1.0), which provides (Xab, Yab) as the
	*   transformed display coordinates. When retrieving this property, ensure
	*   that you have sufficient storage for 9 integers or floats, depending on
	*   the API being used. For integer API only: when you set the @c #SCREEN_PROPERTY_SCALE_FACTOR
	*   property of a window, or device, prior to retrieving this property, you
	*   affect the integer values of this transformation matrix. Floating-point
	*   get/set APIs for this property are unaffected by @c #SCREEN_PROPERTY_SCALE_FACTOR.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: This property is applicable to only input devices of type
	*               @c #SCREEN_EVENT_MTOUCH_TOUCH. When setting this property, the last row of the
	*               transformation matrix must be [0, 0, 65536].
	*     - window: This property can be only retrieved, and not set for a window.
	*     - session: This property can be only retrieved, and not set for a session.
	*/
	SCREEN_PROPERTY_TRANSFORM = 127,

	/** A single integer that provides an alternate key symbol associated to
	*   the event. On reduced keyboards with a limited number of keys, the alternate key symbol
	*   can correspond to a non-printable shortcut or command symbol while the main key symbol
	*   would be suitable for text entry. The alternate key symbol can be identical to the primary
	*   key symbol. When retrieving or setting this property type, ensure that you provide
	*   sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only a @c #SCREEN_EVENT_KEYBOARD event.
	*/
	SCREEN_PROPERTY_KEY_ALTERNATE_SYM = 128,

	/** An integer that is used to identify the display technology used by a particular
	*   object handle. For the list of display technologies types, see
	*   <a href="screen_8h_1Screen_Display_Technology_Types.xml"> Screen display technology types</a>.
	*   When retrieving this property type, ensure that you provide enough storage
	*   for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_TECHNOLOGY = 129,

	/** A single integer bitmask, or a string, indicating the permissions associated with an
	*   API object. As an integer, @c #SCREEN_PROPERTY_PERMISSIONS must be a combination of type
	*   <a href="group__screen__windows_1Screen_Permission_Masks.xml"> Screen permission masks</a>.
	*   As a string, you can refine the permissions more than you can if you set
	*   @c #SCREEN_PROPERTY_PERMISSIONS as an integer.
	*   When retrieving or setting this property as an integer or string, ensure that you provide
	*   sufficient storage for one integer, or a character buffer respectively. Refer to the
	*   @e Screen @e Developer's @e Guide for more information on how to set permissions.
	*   - Configurable: Yes (@c permissions)
	*   - Default privileges: @code rw-rw---- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*     - device
	*     - display
	*     - group
	*     - pixmap
	*     - session
	*     - stream
	*     - window
	*/
	SCREEN_PROPERTY_PERMISSIONS = 131,

	/** A single integer bitmask indicating which modifiers associated with a
	*  device should have a sticky behavior. Refer to @c sys/keycodes.h for valid keyboard
	*  modifiers. A sticky modifier turns on its corresponding locked
	*  modifier automatically, which then gets cleared on the 'key press + key release'
	*  sequence immediately following. When multiple modifiers are set to be
	*  sticky, they can be compounded, allowing \<Shift Down\>, \<Shift Up\>,
	*  \<Alt Down\>, \<Alt Up\>, \<A\> to produce the same results as
	*  \<Shift Down\>, \<Alt Down\>, \<A\>, \<Shift Up\>, \<Alt Up\>. When
	*  retrieving or setting this property type, ensure that you provide
	*  sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: From @c sys/keycodes.h, the valid modifiers for a device are:
	*                 - @c KEYMOD_SHIFT
	*                 - @c KEYMOD_CTRL
	*                 - @c KEYMOD_ALT
	*                 - @c KEYMOD_ALTGR
	*                 - @c KEYMOD_SHL3
	*                 - @c KEYMOD_MOD6
	*                 - @c KEYMOD_MOD7
	*                 - @c KEYMOD_MOD8
	*                 - @c KEYMOD_SHIFT_LOCK
	*                 - @c KEYMOD_CTRL_LOCK
	*                 - @c KEYMOD_ALT_LOCK
	*                 - @c KEYMOD_ALTGR_LOCK
	*                 - @c KEYMOD_SHL3_LOCK
	*                 - @c KEYMOD_MOD6_LOCK
	*                 - @c KEYMOD_MOD7_LOCK
	*                 - @c KEYMOD_MOD8_LOCK
	*                 - @c KEYMOD_CAPS_LOCK
	*                 - @c KEYMOD_NUM_LOCK
	*                 - @c KEYMOD_SCROLL_LOCK
	*/
	SCREEN_PROPERTY_STICKY_MODIFIERS = 132,

	/** A single integer in the range @c [-1, 100000] that controls the brightness of a
	*  display, if that display allows its brightness to be controlled. The default value of
	*  -1 implies that Screen automatically sets the display intensity based on ambient
	*  lighting. When retrieving or setting this property type, ensure that
	*  you provide sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_INTENSITY = 133,

	/** A pointer to a session. When retrieving or setting this property type,
	*   ensure that you have sufficient storage for one @c void pointer.
	*   When you retrieve this property at any point in your application,
	*   Screen locally allocates resources to track this session. You must call @c screen_destroy_session()
	*   to free up these resources when you've finished using this session.
	*
	*   - Configurable: No
	*   - Default privileges: @code rw-rw---- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - device: The session on which the input device is focused. All input from
	*               the device is directed to this particular session.
	*     - event: For the following events, @c #SCREEN_PROPERTY_SESSION
	*              refers to the session associated with the event:
	*              - @c #SCREEN_EVENT_CLOSE
	*              - @c #SCREEN_EVENT_CREATE
	*              .
	*              For the following events, @c #SCREEN_PROPERTY_SESSION
	*              refers to the session associated with the input for which
	*              the event is intended:
	*              - @c #SCREEN_EVENT_GAMEPAD
	*              - @c #SCREEN_EVENT_GESTURE
	*              - @c #SCREEN_EVENT_INPUT
	*              - @c #SCREEN_EVENT_JOG
	*              - @c #SCREEN_EVENT_JOYSTICK
	*              - @c #SCREEN_EVENT_KEYBOARD
	*              - @c #SCREEN_EVENT_MTOUCH_MOVE
	*              - @c #SCREEN_EVENT_MTOUCH_PRETOUCH
	*              - @c #SCREEN_EVENT_MTOUCH_RELEASE
	*              - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*              - @c #SCREEN_EVENT_POINTER
	*              .
	*              For the following event, @c #SCREEN_PROPERTY_SESSION
	*              refers to the session whose property is being set or retrieved:
	*              - @c #SCREEN_EVENT_PROPERTY
	*/
	SCREEN_PROPERTY_SESSION = 137,

	/** A tuple of integers that indicates the minimum and maximum distance, in units produced by speed
	*   and acceleration, for swipes and a timeout value (in milliseconds). This property controls
	*   the amount of displacement allowed before a tap becomes a swipe, and a threshold displacement
	*   at which point a gesture becomes multiple gestures. When retrieving or setting this property type,
	*   ensure that you have sufficient storage for three integers.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - session: The tuple of integers, minimum, maximum, and timeout values are used.
	*     - device: Only the first two integers are used, which are the minimum and maximum values.
	*/
	SCREEN_PROPERTY_GESTURE_PARAMETERS = 139,

	/** A single @c long @c long integer that indicates a time duration,
	*   in nanoseconds. When retrieving or setting this property type, ensure that you have
	*   sufficient storage for one @c long @c long integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only the @c #SCREEN_EVENT_GESTURE event.
	*/
	SCREEN_PROPERTY_DURATION = 140,

	/** A pair of integers that define the x and y coordinates of events with relative displacement.
	*  When retrieving or setting this property type, ensure that you have sufficient storage
	*  for two integers.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property is applicable to only the following events:
	*              - @c #SCREEN_EVENT_GAMEPAD
	*              - @c #SCREEN_EVENT_GESTURE
	*              - @c #SCREEN_EVENT_JOG
	*              - @c #SCREEN_EVENT_JOYSTICK
	*              - @c #SCREEN_EVENT_POINTER
	*/
	SCREEN_PROPERTY_DISPLACEMENT = 141,

	/** Two integers that represents the x, y speed of an input device. When retrieving or setting this
	*   property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: This property is applicable to only the following input device types:
	*               - @c #SCREEN_EVENT_GAMEPAD
	*               - @c #SCREEN_EVENT_JOG
	*               - @c #SCREEN_EVENT_JOYSTICK
	*               - @c #SCREEN_EVENT_POINTER
	*     - session: The type of input session. Valid session types must be of type:
	*               - @c #SCREEN_EVENT_GAMEPAD
	*               - @c #SCREEN_EVENT_JOG
	*               - @c #SCREEN_EVENT_JOYSTICK
	*/
	SCREEN_PROPERTY_SPEED = 142,

	/** An array of six integers that define the acceleration of an input device ([A, B, C, D, E, F]).
	*   The first and second values correspond to the linear coefficients for x and y respectively.
	*   The third and fourth values correspond to the quadratic coefficients for x and y
	*   respectively. The fifth and sixth values are the minimum and maximum thresholds for the
	*   velocity respectively.  Any displacement with a velocity less than the minimum threshold
	*   isn't accelerated. Any displacement with a velocity greater than the  maximum threshold
	*   has its acceleration clamped to the acceleration corresponding to the maximum velocity
	*   threshold. When retrieving or setting this property, ensure that you have sufficient
	*   storage for six integers.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device
	*/
	SCREEN_PROPERTY_ACCELERATION = 143,

	/** A set of 16 integers or floating-point values that represent coefficients
	*   to configure @e ballistic @e filtering. Ballistic filtering minimizes the
	*   noise normally seen on successive touch events that follow a low-speed
	*   ballistic trajectory. The lower the speed of the ballistic movement
	*   (e.g., a stationary finger), the more gain reduction is applied to the
	*   speed of the movement. By doing so, the result is a solid touch with no noise.
	*   Screen passes touch events through ballistic filtering to reduce jitter.
	*   The first 6 values of this property ([A, B, C, D, E, F, ...]) configure a
	*   digital filter while the remaining 10 values are currently unused, but
	*   are reserved for future filter configurations. The coordinates of the
	*   latest touch event are generated by calculating the dot product of the most recent
	*   6 touch events and the 6 ballistic coefficients that are specified in this property.
	*
	*   As integers, the coefficients provided or queried are in fixed-point notation
	*   that's based on the scaling factor @c #SCREEN_PROPERTY_SCALE_FACTOR. For
	*   example, if @c #SCREEN_PROPERTY_SCALE_FACTOR is 16, then the coefficients
	*   are in 16.16 fixed-point notation (i.e., a value of 1 is expressed by
	*   65536). When you set the @c #SCREEN_PROPERTY_SCALE_FACTOR property of a device, prior to
	*   retrieving this property, you affect the values of these ballistic coefficients.
	*
	*   As floats, the coefficients provided or queried are unaffected by the
	*   scaling factor @c #SCREEN_PROPERTY_SCALE_FACTOR.
	*
	*   When retrieving or setting this property, ensure that you have sufficient
	*   storage for 16 integers or floats, depending on the API being used.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device
	*/
	SCREEN_PROPERTY_BALLISTIC_COEFFICIENTS = 144,

	/** A set of 32 integers that are used to customize the mapping of the buttons on an
	*   input device. When retrieving or setting this property, ensure that you have sufficient
	*   storage for 32 integers. For information on the buttons you can use, see
	*   <a href="screen_8h_1Screen_Mouse_Button_Types.xml">Screen mouse button types</a> and
	*   <a href="group__screen__devices_1Screen_Game_Button_Types.xml">Screen game button types</a>.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: This property is applicable to only the following input device types:
	*               - @c #SCREEN_EVENT_GAMEPAD
	*               - @c #SCREEN_EVENT_JOG
	*               - @c #SCREEN_EVENT_JOYSTICK
	*               - @c #SCREEN_EVENT_POINTER
	*/
	SCREEN_PROPERTY_BUTTON_MAP = 146,

	/** An integer specifying which cursor shape to display when the pointer
	*   is over a session. This shape must be one of
	*   <a href="screen_8h_1Screen_Cursor_Shapes.xml"> Screen cursor shapes</a>. When retrieving
	*   or setting this property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - session: The type of input session. Valid session types must be of type
	*                @c #SCREEN_EVENT_POINTER.
	*/
	SCREEN_PROPERTY_CURSOR = 149,

	/** A single integer that defines whether or not the display is currently in
	*   mirror mode. Mirror mode indicates that the internal and external
	*   displays display the same signal. The mirror mode must be of type
	*   <a href="screen_8h_1Screen_Mirror_Types.xml">Screen mirror types</a>.
	*   When retrieving or setting this property type, ensure that you have sufficient
	*   storage for one integer. For more information, see the
	*   "When a display clones the content of another display" section under "Display Geometry" of the
	*   @e Screen @e Developer's @e Guide.
	*   - Configurable: Yes (@c mirroring)
	*   - Default privileges: @code rw-rw---- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*     - event: This property is applicable to only a @c #SCREEN_EVENT_DISPLAY event.
	*/
	SCREEN_PROPERTY_MIRROR_MODE = 150,

	/** A handle to an event. When retrieving this property type, ensure that you provide
	*   sufficient storage for one @c void pointer. A gesture event is provided to you
	*   if the gesture is recognized. For more information on gesture types, see
	*    <a href="screen_8h_1Screen_Gesture_Types.xml">Screen gesture types</a>.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - session: The type of input session. Valid session types must be of type:
	*                - @c #SCREEN_EVENT_JOYSTICK
	*                - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*/
	SCREEN_PROPERTY_EVENT = 151,

	/** A string used either by an object owner or its manager to identify how
	*   the object should be managed.
	*
	*   By managed, we mean that the manager is permitted to set the object's
	*   owner properties, in addition to being able to set the object's
	*   parent properties. For example, being managed means that the manager
	*   can set the size, position, and buffer size for an object.
	*
	*   The manager defines the contents of the string.
	*   After the object sets this property, the object is owned by the parent
	*   or the window manager. The manager, or parent, continues to own this
	*   object until @c #SCREEN_PROPERTY_MANAGER_STRING is set again - this time
	*   by the manager, or parent. The original owner receives a @c #SCREEN_EVENT_PROPERTY
	*   event each time this property is set. The ownership returns to the
	*   original owner. An empty string received by the owner in the
	*   @c #SCREEN_EVENT_PROPERTY event indicates either a failure, or that the
	*   manager, or parent, has declined the management request.
	*
	*   For example, let's say the owner of a window wants the window to be
	*   managed. The owner can either call @c screen_manage_window() or set this
	*   property. After setting this property, the window is managed by its
	*   parent, or the window manager, until the owner receives a
	*   @c #SCREEN_EVENT_PROPERTY event with @c #SCREEN_PROPERTY_MANAGER_STRING
	*   as the property.
	*
	*   When retrieving or setting this property type, ensure that you have
	*   sufficient storage for a character buffer.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_PROPERTY_MANAGER_STRING = 152,

	/** An integer specifying the subtype for a window, input or display manager event.
	*   The value is defined by the manager in question.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event
	*/
	SCREEN_PROPERTY_SUBTYPE = 153,

	/** An integer that indicates the status of a Screen API object. The status must be one of
	*   <a href="screen_8h_1Screen_Status_Types.xml">Screen status types</a>.
	*   When retrieving this property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--rw- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - context
	*     - device
	*     - group
	*     - pixmap
	*     - stream
	*     - session
	*     - window: You can set this property for a window if you have the appropriate permissions.
	*               When setting this property, ensure that you have sufficient storage for
	*               one integer.
	*/
	SCREEN_PROPERTY_STATUS = 154,

	/** A single integer that indicates the number of front buffers that have been
	*   acquired by a consumer. When retrieving this property type, ensure that you have sufficient
	*   storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - stream
	*/
	SCREEN_PROPERTY_FRONT_BUFFER_COUNT = 156,

	/** A handle to a stream. When retrieving or setting this property type,
	*   ensure that you have sufficient storage for one @c void pointer.
	*   When you retrieve this property at any point in your application,
	*   Screen locally allocates resources to track this stream. You must call @c screen_destroy_stream()
	*   to free up these resources when you've finished using this stream.
	*
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property can be only retrieved, and not set for an event object.
	*              For the following events, this property refers to the stream
	*              associated with the event:
	*              - @c #SCREEN_EVENT_CLOSE
	*              - @c #SCREEN_EVENT_CREATE
	*
	*              For the following event, this property refers to the stream whose
	*              property is being set:
	*              - @c #SCREEN_EVENT_PROPERTY
	*     - window: The stream of buffers that are used for rendering and displaying window content.
	*/
	SCREEN_PROPERTY_STREAM = 157,

	/**  The group name or handle that a window's parent is associated with. If the window has
	*    no parent, then the handle is NULL.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - window: The get function you call determines whether this property returns a group
	*               name or a handle:
	*               - @c screen_get_window_property_cv(): The name of the group (the group created
	*                                                     by the parent) that the window has joined.
	*                                                     When retrieving this property type, ensure
	*                                                     that you have sufficient storage for a
	*                                                     character buffer.
	*               - @c screen_get_window_property_pv(): The pointer to a group of type
	*                                                        @c #screen_group_t, that the window's
	*                                                        parent has created. When retrieving
	*                                                        this property type, ensure that you
	*                                                        have sufficient storage for a for a
	*                                                        structure of type @c #screen_group_t.
	*/
	SCREEN_PROPERTY_PARENT = 158,

	/** An array of 256 integers (range from 0 to 65535) that defines the red
	*   gamma mapping table of a display. When retrieving or setting this property type, ensure that
	*   you provide sufficient storage for 256 integers.
	*
	*   You must be working within a privileged context of type @c #SCREEN_DISPLAY_MANAGER_CONTEXT to use this property.
	*
	*   - Configurable: No
	*   - Default privileges: @code r--rw-rw- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_RED_GAMMA_CURVE = 159,

	/** An array of 256 integers (range from 0 to 65535) that defines the green
	*   gamma mapping table of a display. When retrieving or setting this property types, ensure that
	*   you provide sufficient storage for 256 integers.
	*
	*   You must be working within a privileged context of type @c #SCREEN_DISPLAY_MANAGER_CONTEXT to use this property.
	*
	*   - Configurable: No
	*   - Default privileges: @code r--rw-rw- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_GREEN_GAMMA_CURVE = 160,

	/** An array of 256 integers (range from 0 to 65535) that defines the blue
	*   gamma mapping table of a display. When retrieving or setting this property types, ensure that
	*   you provide sufficient storage for 256 integers.
	*
	*   You must be working within a privileged context of type @c #SCREEN_DISPLAY_MANAGER_CONTEXT to use this property.
	*
	*   - Configurable: No
	*   - Default privileges: @code r--rw-rw- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_BLUE_GAMMA_CURVE = 161,

	/** A handle to a pixmap. When retrieving or setting this property type,
	*   ensure that you have sufficient storage for one @c void pointer.
	*   When you retrieve this property at any point in your application,
	*   Screen locally allocates resources to track this pixmap. You must call @c screen_destroy_pixmap()
	*   to free up these resources when you've finished using this pixmap.
	*
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - event: This property can be only retrieved, and not set for an event object.
	*              For the following events, this property refers to the pixmap associated with
	*              the event:
	*              - @c #SCREEN_EVENT_CLOSE
	*              - @c #SCREEN_EVENT_CREATE
	*              .
	*              For the following event, this property refers to the pixmap whose property is
	*              being set:
	*              - @c #SCREEN_EVENT_PROPERTY
	*/
	SCREEN_PROPERTY_PIXMAP = 162,

	/** A single integer containing the number of sessions associated with this
	*   context. When retrieving this property type, ensure that you have sufficient storage for
	*   one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_SESSION_COUNT = 163,

	/** An array of session handles. When retrieving this property type, ensure that you have
	*   sufficient storage for one @c void pointer for each session. Retrieve
	*   the @c #SCREEN_PROPERTY_SESSION_COUNT property to determine the number of sessions that are
	*   associated with this context. Once you know the number of sessions, you can allocate
	*   sufficient storage to retrieve @c #SCREEN_PROPERTY_SESSIONS.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_SESSIONS = 164,

	/** A single integer containing the number of streams associated with this
	*   context. When retrieving this property type, ensure that you have sufficient storage for
	*   one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_STREAM_COUNT = 165,

	/** An array of stream handles. When retrieving this property type, ensure that you have
	*   sufficient storage for one @c void pointer for each stream. Retrieve the
	*   @c #SCREEN_PROPERTY_STREAM_COUNT property to determine the number of streams that are
	*   associated with this context. Once you know the number of streams, you can allocate
	*   sufficient storage to retrieve @c #SCREEN_PROPERTY_STREAMS.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - context
	*/
	SCREEN_PROPERTY_STREAMS = 166,

	/** A single integer that indicates the page code associated with an event.
	*   When retrieving or setting this property type, ensure that you provide sufficient storage
	*   for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*   - event: This property is applicable to only the following events:
	*            - @c #SCREEN_EVENT_KEYBOARD
	*/
	SCREEN_PROPERTY_KEY_PAGE = 167,

	/** A handle to the buffer or buffers associated with the object.
	*   When retrieving this property type, ensure that you provide sufficient storage according
	*   to the API object type.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - pixmap: Only one buffer is allowed for a pixmap object.
	*               When retrieving this property for a pixmap, ensure that
	*               you have sufficient storage for one @c void pointer.
	*     - stream: Multiple buffers may be associated with a stream object.
	*               When retrieving this property for a stream, ensure that
	*               you have sufficient storage for one @c void pointer
	*               for each buffer. Use the @c #SCREEN_PROPERTY_BUFFER_COUNT
	*               property to determine the number of buffers.
	*     - window: Multiple buffers may be associated with a window object.
	*               When retrieving this property for a window, ensure that
	*               you have sufficient storage for one @c void pointer
	*               for each buffer. Use the @c #SCREEN_PROPERTY_BUFFER_COUNT
	*               property to determine the number of buffers.
	*/
	SCREEN_PROPERTY_BUFFERS = 168,

	/** A single @c long @c long integer that indicates the time (in nanoseconds)
	*   before Screen sends the first key repeat event (i.e., @c #SCREEN_EVENT_KEYBOARD events whose
	*   @c #SCREEN_PROPERTY_FLAGS property includes @c #SCREEN_FLAG_KEY_REPEAT) to the context
	*   whose session or window has keyboard focus.
	*   When retrieving or setting this property type, ensure that you have sufficient storage
	*   for one @c long @c long integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: This property is applicable only to devices of type @c #SCREEN_EVENT_KEYBOARD.
	*/
	SCREEN_PROPERTY_REPEAT_DELAY = 169,

	/** A single @c long @c long integer that indicates the time interval (in nanoseconds)
	*   between when Screen sends key repeat events (i.e., @c #SCREEN_EVENT_KEYBOARD events whose
	*   @c #SCREEN_PROPERTY_FLAGS property includes @c #SCREEN_FLAG_KEY_REPEAT) to the context
	*   whose session or window has keyboard focus.
	*   When retrieving or setting this property type, ensure that you have sufficient storage
	*   for one @c long @c long integer.
	*   - Configurable: No
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: This property is applicable only to devices of type @c #SCREEN_EVENT_KEYBOARD.
	*/
	SCREEN_PROPERTY_REPEAT_RATE = 170,

	/** A handle to the buffer or buffers that's been acquired by a consumer.
	*   When retrieving @c #SCREEN_PROPERTY_FRONT_BUFFERS, ensure that
	*   you have sufficient storage for one @c void pointer for each acquired buffer.
	*   Use the property @c #SCREEN_PROPERTY_FRONT_BUFFER_COUNT to determine this number.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - stream
	*/
	SCREEN_PROPERTY_FRONT_BUFFERS = 171,

	/** A single integer containing the number of analog controllers associated
	*   with this object. When retrieving this property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - device: The number of analog controllers associated with this device
	*               (for @c #SCREEN_EVENT_GAMEPAD and @c #SCREEN_EVENT_JOYSTICK devices only).
	*/
	SCREEN_PROPERTY_ANALOG_COUNT = 172,

	/** A single integer that indicates the @c #SCREEN_PROPERTY_ID of the context that is registered
	*   as the display's window manager.  A value of @c #SCREEN_INVALID_ID indicates there is no
	*   window manager registered for that display. When retrieving this property type, ensure that
	*   you have sufficient storage for one integer.
	*   This property is associated with the preprocessor definition @c SCREEN_PROPERTY_WINDOW_MANAGER_ID.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_WINDOW_MANAGER_ID = 173,

	/** A single integer that indicates the file descriptor to be used to access
	*   a buffer's memory.  Currently, this can only be used to provide memory to
	*   Screen.  When setting this property type, ensure that you have sufficient
	*   storage for one integer.
	*   This property is associated with the preprocessor definition @c SCREEN_PROPERTY_FD.
	*   - Configurable: No
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - buffer
	*/
	SCREEN_PROPERTY_FD = 174,

	/** A single integer that indicates the input sensitivity alpha threshold. The
	*   sensitivity threshold must be an integer in the range 0 to 0xFF (0 to 255).
	*   The default sensitivity threshold is 0. When retrieving or setting this
	*   property type, ensure that you have sufficient storage for one integer.
	*   When sensitivity threshold is set, the alpha component of the selected
	*   pixel will be compared to the sensitivity threshold; if the alpha is less
	*   than or equal to the threshold, the input event will be treated as
	*   transparent, otherwise if the alpha is greater than the threshold, the
	*   input event will be consumed. Care must be taken to set properly upscaled
	*   sensitivity threshold values when using formats which have alpha components
	*   that do not span the full 0 to 0xFF range. The Screen property
	*   @c #SCREEN_PROPERTY_SENSITIVITY takes priority over the sensitivity
	*   threshold setting.
	*   - Configurable: Yes (sensitivity-threshold)
	*   - Default privileges: @code rw-r--r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - session
	*     - window
	*/
	SCREEN_PROPERTY_SENSITIVITY_THRESHOLD = 175,

	/** A set of integers or floating-point values that define the chromaticity
	*   coordinates for a display's output in CIE 1931 color-space xy coordinate
	*   format. When queried as integers, the values retrieved are in fixed-point
	*   representation with 31 bits of sub-pixel precision. When retrieving this
	*   property, ensure that you have sufficient storage for 8 integers or floating-point
	*   values, depending on the API being used. The values returned when retrieving
	*   this property are the [x,y] coordinate pairs for each of the following color
	*   points: red(R), green(G), blue(B), white(D65), in that order. The default
	*   chromaticity coordinates for a given display are platform-dependent.
	*   - Configurable: No
	*   - Default privileges: @code r--r--r-- @endcode
	*   - Change event sent: No
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_CHROMATICITY_COORDS = 176,

	/** A single integer that specifies the focus mode of the display.
	*
	*   The enums SCREEN_FOCUS_MODE_AUTOMATIC and SCREEN_FOCUS_MODE_MANUAL can be used
	*   as input for this property. By default SCREEN_FOCUS_MODE_AUTOMATIC applies to displays.
	*
	*   You must be working within a privileged context of type @c #SCREEN_DISPLAY_MANAGER_CONTEXT to use this property.
	*
	*   When retrieving or setting this property type, ensure that you have sufficient storage for one integer.
	*   - Configurable: Yes
	*   - Default privileges: @code rw-rw-r-- @endcode
	*   - Change event sent: Yes
	*   - Applicable to the following Screen API object(s):
	*     - display
	*/
	SCREEN_PROPERTY_FOCUS_MODE = 177,

	/** This MUST be the final value in the enum; do not use. */
	SCREEN_MAX_PROPERTY
};

/** @brief The flags that are supported by display mode objects.
 *  @anonenum Screen_Display_Mode_Flags Screen display mode flags
 */
enum {
	/** The indicator that specifies the display mode is the preferred mode. Used in the @c flags
	*   of the type @c #screen_display_mode_t.
	*/
	SCREEN_DISPLAY_MODE_PREFERRED = 0x1,

	/** The indicator that specifies that the display mode format value is valid. Used in the @c flags
	*   of the type @c #screen_display_mode_t.
	*/
	SCREEN_DISPLAY_MODE_FORMAT_VALID = 0x2,

	/** The indicator that specifies that the display mode refresh value is valid. Used in the @c flags
	*   of the type @c #screen_display_mode_t.
	*/
	SCREEN_DISPLAY_MODE_REFRESH_VALID = 0x4
};

/** @brief The color space types that are supported by window, stream, pixmap,
*   and buffer objects.
*   @anonenum Screen_Color_Space_Types Screen color space types
*/
enum {
	/** Only historically called "uncorrected"; this does not reflect the
	*   true behavior of this mode. This color space mode means no
	*   transformations applied to the RGB data, and some unspecified
	*   transfer function is applied to the YUV data. Most drivers assume
	*   BT.601 color space in this mode for YUV.
	*/
	SCREEN_COLOR_SPACE_UNCORRECTED  = 0x0,

	/** The raw RGB data is in sRGB color space defined by the IEC 61966-2-1:1999.
	*/
	SCREEN_COLOR_SPACE_SRGB         = 0x1,

	/** The raw RGB data is in linear RGB color space.
	*/
	SCREEN_COLOR_SPACE_LRGB         = 0x2,

	/** The raw YUV data is in BT.601 color space defined by the BT.601
	*   standard https://www.itu.int/rec/R-REC-BT.601 . In this color space
	*   the luminance signal is occupying 220 (8-bit) or 877 (10-bit)
	*   levels and the colour-difference signals are occupying 225 (8-bit)
	*   or 897 (10-bit) levels. For 8-bit: Y is in range 16..235, U and V
	*   is in range 16..240. For 10-bit: Y is in range 64..940, U and V is
	*   in range 64..960.
	*/
	SCREEN_COLOR_SPACE_BT601        = 0x3,

	/** The raw YUV data is in color space described by the ITU-T T.871
	*   which is based on the BT.601 color space defined by the BT.601
	*   standard. Sometimes it is called JPEG color space. In this color
	*   space the luminance signal is occupying all 256 (8-bit) or 1024
	*   (10-bit) levels. For 8-bit: Y is in range 0..255, U and V is in
	*   range 0.255. For 10-bit: Y is in range 0..1023, U and V is in
	*   range 0..1023.
	*/
	SCREEN_COLOR_SPACE_BT601_FULL   = 0x4,

	/** The raw YUV data is in BT.709 color space defined by the BT.709
	*   standard https://www.itu.int/rec/R-REC-BT.709 . In this color space
	*   the luminance signal is occupying 220 (8-bit) or 877 (10-bit)
	*   levels and the colour-difference signals are occupying 225 (8-bit)
	*   or 897 (10-bit) levels. For 8-bit: Y is in range 16..235, U and V
	*   is in range 16..240. For 10-bit: Y is in range 64..940, U and V is
	*   in range 64..960.
	*/
	SCREEN_COLOR_SPACE_BT709        = 0x5,

	/** There is no dedicated standard for this color space. It is based on
	*   the BT.709 color space defined by the BT.709 standard. In this color
	*   space the luminance signal is occupying all 256 (8-bit) or 1024
	*   (10-bit) levels. For 8-bit: Y is in range 0..255, U and V is in
	*   range 0.255. For 10-bit: Y is in range 0..1023, U and V is in
	*   range 0..1023.
	*/
	SCREEN_COLOR_SPACE_BT709_FULL   = 0x6,  /**< Refer to BT.709 standards */

	/** The raw YUV data is in BT.2020 color space defined by the BT.2020
	*   standard https://www.itu.int/rec/R-REC-BT.2020 . This color space
	*   doesn't standardize 8 bit data, only 10 and 12 bits. For 10-bit:
	*   Y is in range 64..940, U and V is in range 64..960. For 12-bit:
	*   Y is in range 256..3760, U and V is in range 256..3840.
	*/
	SCREEN_COLOR_SPACE_BT2020       = 0x7,  /**< Refer to BT.2020 standards */

	/** There is no dedicated standard for this color space. It is based on
	*   the BT.2020 color space defined by the BT.2020 standard. This color
	*   space doesn't standardize 8 bit data, only 10 and 12 bits. For 10-bit:
	*   Y is in range 0..1023, U and V is in range 0..1023. For 12-bit:
	*   Y is in range 0..4095, U and V is in range 0..4095.
	*/
	SCREEN_COLOR_SPACE_BT2020_FULL  = 0x8,  /**< Refer to BT.2020 standards */

	/** This MUST be the final value in the enum; do not use. */
	SCREEN_MAX_COLOR_SPACE
};

/**
*   @brief Defines the display mode preferred index
*
*   Used as a convenience value when using screen_set_display_property_iv() to set the
*   @c #SCREEN_PROPERTY_MODE property. When you use this value, Screen uses the default video mode
*   without you having to first determine all the modes supported by the display to find the one with
*   @c #SCREEN_DISPLAY_MODE_PREFERRED set in @c flags.
*/
#define SCREEN_DISPLAY_MODE_PREFERRED_INDEX (-1)

/**
*   @brief Deprecated; use @c #SCREEN_DISPLAY_MODE_PREFERRED_INDEX instead.
*/
#define SCREEN_MODE_PREFERRED_INDEX (SCREEN_DISPLAY_MODE_PREFERRED_INDEX)

/**
*    A structure to contain values related to Screen display mode.
*/
typedef struct _screen_mode {
	_Uint32t width;             /**< Width in pixels. */
	_Uint32t height;            /**< Height in pixels. */
	_Uint32t refresh;           /**< Refresh rate in Hz.  Refresh when used when creating a virtual display. The <a href="../screen_8h_1Screen_Mode_Types.xml"> SCREEN_DISPLAY_MODE_REFRESH_VALID</a> flag must be seti when create virtual display. */
	_Uint32t interlaced;        /**< Interlace mode. 1 for interlace, 0 for no interlace. Not applicable on virtual displays. */
	_Uint32t aspect_ratio[2];   /**< Aspect ratio. */
	_Uint32t flags;             /**< See  <a href="screen_8h_1Screen_Mode_Types.xml">Screen mode types</a> for more information. */
	_Uint32t index;             /**< An index from the mode list retrieved using <a href="../screen_get_display_modes.xml">screen_get_display_modes()</a>. */
	_Int32t  format;            /**< Format used when creating a virtual display. The <a href="../screen_8h_1Screen_Mode_Types.xml"> SCREEN_DISPLAY_MODE_FORMAT_VALID</a> flag must be set. */
	_Uint32t reserved[5];       /**< Reserved bits */
} screen_display_mode_t;

/** @brief Types of Screen API objects
 *  @section Screen_Object_Types Screen object types
 */
enum {
	SCREEN_OBJECT_TYPE_CONTEXT             = 0, /**< Screen context object. */
	SCREEN_OBJECT_TYPE_GROUP               = 1, /**< Screen group object. */
	SCREEN_OBJECT_TYPE_DISPLAY             = 2, /**< Screen display object. */
	SCREEN_OBJECT_TYPE_DEVICE              = 3, /**< Screen device object. */
	SCREEN_OBJECT_TYPE_PIXMAP              = 4, /**< Screen pixmap object. */
	SCREEN_OBJECT_TYPE_SESSION             = 6, /**< Screen session object. */
	SCREEN_OBJECT_TYPE_STREAM              = 7, /**< Screen stream object. */
	SCREEN_OBJECT_TYPE_WINDOW              = 8, /**< Screen window object. */
};

/**
 *  @brief The value of the @c #SCREEN_PROPERTY_WINDOW_MANAGER_ID property of a display object
 *         when there's no window manager associated with it.
 */
#define SCREEN_INVALID_ID (-1)

/** @brief Types of power modes
 *  @anonenum Screen_Power_Mode_Types Screen power mode types
 */
enum {
	/** The power mode in an inactive state. */
	SCREEN_POWER_MODE_OFF = 0x7680,

	/** The power mode in a state of being partially off; the display
	*   or device is no longer active. The power usage in this state can be
	*   greater than in @c #SCREEN_POWER_MODE_OFF, but will allow for a
	*   faster transition to active state.
	*/
	SCREEN_POWER_MODE_SUSPEND = 0x7681,

	/** The power mode in a state of reduced power; the display or
	*   device is active, but may be slower to update than if it was in
	*   @c #SCREEN_POWER_MODE_ON.
	*/
	SCREEN_POWER_MODE_LIMITED_USE = 0x7682,

	/** The power mode in an active state. */
	SCREEN_POWER_MODE_ON = 0x7683,
};

/** @brief Types of idle modes
 *  @anonenum Screen_Idle_Mode_Types Screen idle mode types
 */
enum {
	/** The default idle mode; the display is allowed to go idle after
	*   the period of time indicated by @c #SCREEN_PROPERTY_IDLE_TIMEOUT
	*   and potentially turn off.
	*/
	SCREEN_IDLE_MODE_NORMAL = 0,

	/** The idle mode which will prevent the display from going idle
	*   after a period of no input - such as video playback. By default, the
	*   display will go idle after the period of time indicated by
	*   @c #SCREEN_PROPERTY_IDLE_TIMEOUT.
	*/
	SCREEN_IDLE_MODE_KEEP_AWAKE = 1,
};

/** @brief Types of connections to a display
*   @anonenum Screen_Display_Types Screen display types
*/
enum {
	/** An internal connection type to the display */
	SCREEN_DISPLAY_TYPE_INTERNAL = 0x7660,

	/** A composite connection type to the display */
	SCREEN_DISPLAY_TYPE_COMPOSITE = 0x7661,

	/** A S-Video connection type to the display */
	SCREEN_DISPLAY_TYPE_SVIDEO = 0x7662,

	/** A component connection type to the display - specifically
	*   the YPbPr signal of the component connection.
	*/
	SCREEN_DISPLAY_TYPE_COMPONENT_YPbPr = 0x7663,

	/** A component connection type to the display - specifically
	*   the RGB signal of the component connection.
	*/
	SCREEN_DISPLAY_TYPE_COMPONENT_RGB = 0x7664,

	/** A component connection type to the display - specifically
	*   the RBGHV signal of the component connection.
	*/
	SCREEN_DISPLAY_TYPE_COMPONENT_RGBHV = 0x7665,

	/** A DVI connection type to the display */
	SCREEN_DISPLAY_TYPE_DVI = 0x7666,

	/** A HDMI connection type to the display */
	SCREEN_DISPLAY_TYPE_HDMI = 0x7667,

	/** A DisplayPort connection type to the display */
	SCREEN_DISPLAY_TYPE_DISPLAYPORT = 0x7668,

	/** A connection type to the display which is one other than
	*   internal, composite, S-Video, component, DVI, HDMI, or DisplayPort.
	*/
	SCREEN_DISPLAY_TYPE_OTHER = 0x7669,

	/** A MIPI DSI connection type to the display; this connection type is
	*   associated with the preprocessor definition @c SCREEN_DISPLAY_TYPE_DSI
	*/
	SCREEN_DISPLAY_TYPE_DSI = 0x7675,
};

/** @brief Types of technologies for a display
*   @anonenum Screen_Display_Technology_Types Screen display technology types
*/
enum {
	/** Any other display technology that isn't enumerated. */
	SCREEN_DISPLAY_TECHNOLOGY_UNKNOWN = 0,

	/** All monochrome and standard tricolor CRTs. */
	SCREEN_DISPLAY_TECHNOLOGY_CRT = 1,

	/** All active and passive matrix LCDs. */
	SCREEN_DISPLAY_TECHNOLOGY_LCD = 2,

	/** All PDP types including DC and AC plasma displays. */
	SCREEN_DISPLAY_TECHNOLOGY_PLASMA = 3,

	/** Inorganic LED. */
	SCREEN_DISPLAY_TECHNOLOGY_LED = 4,

	/** Organic LED/OEL. */
	SCREEN_DISPLAY_TECHNOLOGY_OLED = 5,
};

/** @brief Types of mirrors
 *  @anonenum Screen_Mirror_Types Screen mirror types
 */
enum {
	/** Mirroring is disabled. */
	SCREEN_MIRROR_DISABLED = 0,

	/** Mirroring is enabled, and the aspect-ratio of the image is 1:1.*/
	SCREEN_MIRROR_NORMAL = 1,

	/** Mirroring is enabled, and the image fills the display while preserving the aspect-ratio.
	*   Image may be shown with black bars where applicable.
	*/
	SCREEN_MIRROR_STRETCH = 2,

	/** Mirroring is enabled, and the image fills the display while preserving the aspect-ratio.
	*   Image content may be clipped.
	*/
	SCREEN_MIRROR_ZOOM = 3,

	/** Mirroring is enabled, and the image fills the display while not preserving the
	*   aspect-ratio.
	*/
	SCREEN_MIRROR_FILL = 4,
};

/** @brief Types of scale modes
 *  @anonenum Screen_Scale_Types Screen scale types
 */
enum {
	/** Scaling is disabled, and the aspect-ratio of the image is 1:1.*/
	SCREEN_SCALE_NONE = 1,

	/** Scaling is enabled, and the image fills the display while preserving the aspect-ratio.
	*   Image may be shown with black bars where applicable.
	*/
	SCREEN_SCALE_STRETCH = 2,

	/** Scaling is enabled, and the image fills the display while preserving the aspect-ratio.
	*   Image content may be clipped.
	*/
	SCREEN_SCALE_ZOOM = 3,

	/** Scaling is enabled, and the image should fill the display while not preserving the
	*   aspect-ratio.
	*/
	SCREEN_SCALE_FILL = 4,

	/**
	* Scaling is enabled, and the image is rendered to fit the window while shifted half a line up.
	*/
	SCREEN_SCALE_HALF_LINE_SHIFT_UP = 5,

	/**
	* Scaling is enabled, and the image is rendered to fit the window while shifted half a line down.
	*/
	SCREEN_SCALE_HALF_LINE_SHIFT_DOWN = 6
};

/** @brief Types of available alpha modes
*   @anonenum Screen_Alpha_Mode_Types Screen Alpha Mode Types
*/
enum {
	/**  The non pre-multiplied alpha content. This is the default.
	*    In this case, the source blending is done using the equation:
	*
	*    c(r,g,b) = s(r,g,b) * s(a) + d(r,g,b) * (1 - s(a))
	**/
	SCREEN_NON_PRE_MULTIPLIED_ALPHA = 0,

	/**  The pre-multiplied alpha content.  In this case, the source blending is
	*    done using the equation:
	*
	*    c(r,g,b) = s(r,g,b) + d(r,g,b) * (1 - s(a))
	**/
	SCREEN_PRE_MULTIPLIED_ALPHA = 1,
};

/** @brief Types of supported pixel formats
*
*   Formats with an alpha channel will have source alpha enabled automatically.
*   Applications that want the Screen API to disregard the alpha channel can
*   choose a pixel format with an @e X.
*   @anonenum Screen_Pixel_Format_Types Screen pixel format types
*/
enum {
	/** This is a multipurpose byte-based array of data. It does not represent any actual pixel format.*/
	SCREEN_FORMAT_BYTE = 1,

	/** 16 bits per pixel (4 bits per channel) RGB with alpha channel */
	SCREEN_FORMAT_RGBA4444 = 2,

	/** 16 bits per pixel (4 bits per channel) RGB with alpha channel
	*   disregarded
	*/
	SCREEN_FORMAT_RGBX4444 = 3,

	/** 16 bits per pixel, 2 bytes containing R, G, and B values
	* (5 bits per channel with single-bit alpha channel)
	*/
	SCREEN_FORMAT_RGBA5551 = 4,

	/** 16 bits per pixel, 2 bytes containing R, G, and B values
	* (5 bits per channel with single-bit alpha channel disregarded)
	*/
	SCREEN_FORMAT_RGBX5551 = 5,

	/** 16 bits per pixel; uses five bits for red, six bits for green and
	*   five bits for blue. This pixel format represents each pixel in the
	*   following order (high byte to low byte): RRRR RGGG GGGB BBBB
	*/
	SCREEN_FORMAT_RGB565 = 6,

	/** 24 bits per pixel (8 bits per channel) RGB */
	SCREEN_FORMAT_RGB888 = 7,

	/** 32 bits per pixel (8 bits per channel) RGB with alpha channel */
	SCREEN_FORMAT_RGBA8888 = 8,

	/** 32 bits per pixel (8 bits per channel) RGB with alpha channel disregarded */
	SCREEN_FORMAT_RGBX8888 = 9,

	/** 9 bits per pixel planar YUV format. 8-bit Y plane and
	*   8-bit 4x4 subsampled U and V planes. Registered by Intel.
	*/
	SCREEN_FORMAT_YVU9 = 10,

	/** 12 bits per pixel planar YUV format. 8-bit Y plane
	*   and 8-bit 2x2 subsampled U and V planes. Alias names are YU12, I420.
	*/
	SCREEN_FORMAT_YUV420 = 11,

	/** 12 bits per pixel planar YUV format. 8-bit Y plane
	*   and 2x2 subsampled, interleaved U and V planes.
	*/
	SCREEN_FORMAT_NV12 = 12,

	/** 12 bits per pixel planar YUV format. 8-bit Y plane
	*   and 8-bit 2x2 subsampled V and U planes.
	*/
	SCREEN_FORMAT_YV12 = 13,

	/** 16 bits per pixel packed YUV 4:2:2 format. Y sampled at every
	*   pixel, U and V sampled at every second pixel horizontally on each line.
	*   Byte order in memory: U[8]:Y1[8]:V[8]:Y2[8].
	*/
	SCREEN_FORMAT_UYVY = 14,

	/** 16 bits per pixel packed YUV 4:2:2 format. Y sampled at every
	*   pixel, U and V sampled at every second pixel horizontally on each line.
	*   Byte order in memory: Y1[8]:U[8]:Y2[8]:V[8]. Alias name is YUYV.
	*/
	SCREEN_FORMAT_YUY2 = 15,

	/** 16 bits per pixel packed YUV 4:2:2 format. Y sampled at every
	*   pixel, U and V sampled at every second pixel horizontally on each line.
	*   Byte order in memory: Y1[8]:V[8]:Y2[8]:U[8].
	*/
	SCREEN_FORMAT_YVYU = 16,

	/** 16 bits per pixel packed YUV 4:2:2 format. Y sampled at every
	*   pixel, U and V sampled at every second pixel horizontally on each line.
	*   Byte order in memory: V[8]:Y1[8]:U[8]:Y2[8]. Alias name is VYUY.
	*/
	SCREEN_FORMAT_V422 = 17,

	/**  32 bits packed YUV format similar to RGBA layout. Byte order in memory is
	*    V[8]:U[8]:Y[8]:A[8], when fetched on little-endian machines as 32 bit
	*    integer layout will be the following: A[31-24]:Y[23-16]:U[15-8]:V[7-0].
	*/
	SCREEN_FORMAT_AYUV = 18,

	/** 8-bit Y plane; 2x1-subsampled, interleaved U and V planes
	*   (Like NV12 but with chroma for each row); this format is
	*   associated with the preprocessor definition @c SCREEN_FORMAT_NV16
	*/
	SCREEN_FORMAT_NV16 = 19,

	/** 24 bits per macro pixel planar YUV 4:2:0 format. 16-bit Y plane with effective
	*   10 most significant bits and 2x2 subsampled, interleaved 16 bit with effective
	*   10 most significant bits U and V planes. It's fully identical to NV12, but every
	*   component occupies 16 bits instead of 8 bits. YUV components must be treated as
	*   fixed point in 8.2 format.
	*/
	SCREEN_FORMAT_P010 = 20,

	/** 32 bits per pixel (8 bits per channel) BGR with alpha channel */
	SCREEN_FORMAT_BGRA8888 = 21,

	/** 32 bits per pixel (8 bits per channel) BGR with alpha channel disregarded */
	SCREEN_FORMAT_BGRX8888 = 22,

	/** A four-component, 32-bit unsigned integer format that supports
	*   10 bits for each color and 2 bits for alpha. The alpha channel is located
	*   at bits position 30-31, the red channel is defined in the most significant
	*   bits (bits position 20-29) */
	SCREEN_FORMAT_RGBA1010102 = 23,

	/** A four-component, 32-bit unsigned integer format that supports
	*   10 bits for each color and 2 unused MSB bits. The red channel is defined
	*   in the most significant bits (bits position 20-29) */
	SCREEN_FORMAT_RGBX1010102 = 24,

	/** A four-component, 32-bit unsigned integer format that supports
	*   10 bits for each color and 2 bits for alpha. The alpha channel is located
	*   at bits position 30-31, the red channel is defined in the least significant
	*   bits (bits position 0-9) */
	SCREEN_FORMAT_BGRA1010102 = 25,

	/** A four-component, 32-bit unsigned integer format that supports
	*   10 bits for each color and 2 unused MSB bits. The red channel is defined
	*   in the least significant  bits (bits position 0-9) */
	SCREEN_FORMAT_BGRX1010102 = 26,

	/** Number of pixel formats plus 1 (0 is not a valid format). */
	SCREEN_FORMAT_NFORMATS,
};

/** @brief Types of usage flags
*
*   Usage flags are used when allocating buffers; they indicate to Screen how the application
*   intends to use the buffers. Depending on the usage, different constraints such as width, height,
*   stride granularity or special alignment must be observed. The usage is also valuable in
*   determining the amount of caching that can be set on a particular buffer.
*   @anonenum Screen_Usage_Flag_Types Screen usage flag types
*/
enum {
	/** Internal use only; do not use. */
	SCREEN_USAGE_DISPLAY = (1 << 0),

	/** Flag to indicate that the CPU can read from the buffer(s) associated with the
	*   API object.
	*/
	SCREEN_USAGE_READ = (1 << 1),

	/** Flag to indicate that the CPU can write to the buffer(s) associated with the
	*   API object.
	*/
	SCREEN_USAGE_WRITE = (1 << 2),

	/** Flag to indicate that buffer(s) associated with the API object can be
	*   used for native API operations. If using blits (@c screen_blit()) or
	*   fills (@c screen_fill()), this flag must be set on the buffer.
	*/
	SCREEN_USAGE_NATIVE = (1 << 3),

	/** Flag to indicate that OpenGL ES 2.X is used for rendering the buffer
	*   associated with the API object.
	*/
	SCREEN_USAGE_OPENGL_ES2 = (1 << 5),

	/** Flag to indicate that OpenGL ES 3.X is used for rendering the buffer
	*   associated with the API object.
	*/
	SCREEN_USAGE_OPENGL_ES3 = (1 << 11),

	/** Flag to indicate that OpenVG is used for rendering the buffer
	*   associated with the API object.
	*/
	SCREEN_USAGE_OPENVG = (1 << 6),

	/** Flag to indicate that the buffer can be written to by a video decoder.
	*/
	SCREEN_USAGE_VIDEO = (1 << 7),

	/** Flag to indicate that the buffer can be written to by capture devices
	*   (such as cameras, analog-to-digital-converters, ...), and read by
	*   a hardware video encoder.
	*/
	SCREEN_USAGE_CAPTURE = (1 << 8),

	/** Flag to indicate that the buffer can be reconfigured from landscape to
	*   portrait orientation without reallocation.
	*/
	SCREEN_USAGE_ROTATION = (1 << 9),

	/** Flag to suggest the use of a display overlay.
	*   Screen won't use display overlays unless your application assigns a
	*   pipeline. The @c #SCREEN_USAGE_OVERLAY flag suggests that Screen tries
	*   to post your buffer onto a pipeline that doesn't have a framebuffer. If
	*   unable to, Screen will use composition instead. Changing
	*   @c #SCREEN_USAGE_OVERLAY may affect pipeline assignment. Set
	*   this flag when you are targeting a non-composited pipeline.
	*/
	SCREEN_USAGE_OVERLAY = (1 << 10),

	/** Flag to suggest that a compressed version of the buffer format be used. This compressed version
	*   might have lower memory requirements per buffer. The buffer contents have a different layout
	*   than a buffer in the uncompressed version.
	 */
	SCREEN_USAGE_COMPRESSION = (1 << 12),

	/** Flag to indicate that the buffer should be physically contiguous
	*   and the physical address should be provided.
	*   This flag is used only as a suggestion to Screen. Query the buffer's
	*   @c #SCREEN_PROPERTY_PHYSICALLY_CONTIGUOUS property to confirm whether the buffer is actually contiguous.
	*   This flag is associated with the preprocessor definition @c SCREEN_USAGE_PHYSICAL.
	*/
	SCREEN_USAGE_PHYSICAL = (1 << 13),

	/** Flag to indicate that Vulkan is used for rendering the buffer
	*   associated with the API object.
	*/
	SCREEN_USAGE_VULKAN = (1 << 14),

	/** Flag to indicate that buffers must be allocated as protected by OS.
	*/
	SCREEN_USAGE_PROTECTION = (1 << 15),
};

/** @brief Types of window transparencies
 *  @anonenum Screen_Transparency_Types Screen transparency types
 */
enum {
	/** Destination pixels are replaced by source pixels, including the alpha
	*   channel.
	*/
	SCREEN_TRANSPARENCY_SOURCE = 0,

	/** Destination pixels are replaced by source pixels. The replacement is
	*   based on a comparison between the source pixel's alpha value and the reference alpha value.
	*   The reference alpha value is dependent on the blitter.
	*/
	SCREEN_TRANSPARENCY_TEST,

	/** Destination pixels are replaced by source pixels when the source color does not match
	*   the reference color value (0xffff00ff). When the source color does match the reference color value,
	*   the pixels are treated as transparent.
	*/
	SCREEN_TRANSPARENCY_SOURCE_COLOR,

	/** Typical alpha blending; the source pixels are blended over the destination pixels.
	*/
	SCREEN_TRANSPARENCY_SOURCE_OVER,

	/** Destination pixels are replaced by fully-visible source pixels.
	*/
	SCREEN_TRANSPARENCY_NONE,

	/** Source is considered completely transparent; the destination is not modified.
	*/
	SCREEN_TRANSPARENCY_DISCARD,

	/** Destination pixels are replaced by source pixels. The replacement is
	*   based on a comparison between the source pixel's alpha value and the reference alpha value.
	*   The reference alpha value is dependent on the blitter.
	*/
	SCREEN_TRANSPARENCY_REVERSED_TEST,

	/** The source pixels are blended over the destination pixels as in the SCREEN_TRANSPARENCY_SOURCE_OVER.
	*   The final destination alpha value is set to opaque on render target write.
	*/
	SCREEN_TRANSPARENCY_SOURCE_OVER_OPAQUE,

	/** Total amount of transparency modes.
	*/
	SCREEN_TRANSPARENCY_MAX,
};

/** @brief Types of rotations
 *  @anonenum Screen_Rotation_Types Screen rotation types
 */
enum {
    /** No rotation */
	SCREEN_ROTATION_NONE = 0,

	/** A 90 degree rotation clockwise */
	SCREEN_ROTATION_90 = 90,

	/** A 180 degree rotation clockwise */
	SCREEN_ROTATION_180 = 180,

    /** A 270 degree rotation clockwise */
	SCREEN_ROTATION_270 = 270,
};

/** @brief Types of sensitivities
 *  @details We recommend that you use a bitmask combination of
 *           <a href="screen_8h_1Screen_Sensitivity_Masks.xml">Screen sensitivity masks</a>
 *           to specify #SCREEN_PROPERTY_SENSITIVITY instead.
 *  @anonenum Screen_Sensitivity_Types Screen sensitivity types
 */
enum {
	/**
	*   The default sensitivity. Pointer and multi-touch events are
	*   forwarded to the window's context if they intersect with the window and
	*   are in an area of the window that's not fully transparent. The window
	*   receives keyboard, gamepad, joystick events if it has input focus.
	*   Raising a window, pointer, or multi-touch release event in the window
	*   causes it to acquire input focus.
	*/
	SCREEN_SENSITIVITY_TEST = 0,

	/**
	*    Pointer and touch events are always forwarded to the
	*    window's context if they intersect with the window, even if the window
	*    is transparent in that area. The window receives keyboard, gamepad,
	*    joystick events if it has input focus. Raising a window, pointer, or
	*    multi-touch release event in that window causes it to acquire input
	*    focus. We recommend that you use #SCREEN_SENSITIVITY_MASK_ALWAYS instead.
	*/
	SCREEN_SENSITIVITY_ALWAYS = 1,

	/**
	*    The window never receives pointer or multi-touch events.
	*    The window never acquires input focus, even after it has been raised.
	*    The window receives only input events that are directly injected into
	*    it from outside sources. We recommend that you use
	*    (#SCREEN_SENSITIVITY_MASK_NEVER | #SCREEN_SENSITIVITY_MASK_NO_FOCUS) instead.
	*/
	SCREEN_SENSITIVITY_NEVER = 2,

	/**
	*    Pointer and touch events are forwarded to the window's
	*    context if they intersect the window and are in an area of the window
	*    that's not fully transparent. The window doesn't acquire input focus
	*    after being raised or after a pointer or multi-touch release event
	*    occurs. Therefore, the window won't receive keyboard, gamepad, or
	*    joystick input unless it's sent directly into the window from an
	*    outside source. We recommend that you use #SCREEN_SENSITIVITY_MASK_NO_FOCUS instead.
	*/
	SCREEN_SENSITIVITY_NO_FOCUS = 3,

	/**
	*    Pointer and touch events are forwarded to the window's
	*    context no matter where they are on the display. The window is considered
	*    fullscreen for the purposes of input tests. Transparency is
	*    ignored. The window receives keyboard, gamepad, and joystick events
	*    if it has input focus. Raising the window, pointer, or multi-touch
	*    release event in the window causes it to acquire input focus. We recommend that you use
	*    (#SCREEN_SENSITIVITY_MASK_ALWAYS | #SCREEN_SENSITIVITY_MASK_FULLSCREEN) instead.
	*/
	SCREEN_SENSITIVITY_FULLSCREEN = 4,
};

/** @brief Types of sensitivity masks
*
*    These masks are intended to be combined into a single integer bitmask
*    representing combinations of desired sensitivities to be applied to a
*    window.
*    @anonenum Screen_Sensitivity_Masks Screen sensitivity masks
*/
enum {
	/**
	*    Pointer and touch events are always forwarded to the
	*    window's context if they intersect with the window - regardless of
	*    transparency. The window receives keyboard, gamepad, or
	*    joystick events if it has input focus. Raising a window, pointer or
	*    multi-touch release event in that window causes it to acquire input
	*    focus. This mask can't be used with #SCREEN_SENSITIVITY_MASK_NEVER.
	*/
	SCREEN_SENSITIVITY_MASK_ALWAYS = (1 << 0),

	/**
	*    The window never receives pointer or multi-touch events.
	*    The window never acquires input focus, even after it has been raised.
	*    The window receives only input events that are directly injected into
	*    it from outside sources. This mask can't be used with #SCREEN_SENSITIVITY_MASK_ALWAYS.
	*/
	SCREEN_SENSITIVITY_MASK_NEVER = (2 << 0),

	/**
	*    Pointer and touch events are forwarded to the window's
	*    context if they intersect the window and are in an area of the window
	*    that isn't fully transparent. The window doesn't acquire input focus
	*    after being raised or after a pointer or multi-touch release event
	*    occurs. Therefore, the window won't receive keyboard, gamepad, or
	*    joystick input unless it's sent directly into the window from an
	*    outside source.
	*/
	SCREEN_SENSITIVITY_MASK_NO_FOCUS = (1 << 3),

	/**
	*    Pointer and touch events are forwarded to the window's
	*    context no matter where they are on the display. The window is considered
	*    fullscreen for the purposes of input tests. Transparency is
	*    ignored. The window receives keyboard, gamepad, and joystick events
	*    as long as the window is visible.
	*/
	SCREEN_SENSITIVITY_MASK_FULLSCREEN = (1 << 4),

	/**
	*   Windows underneath this window can receive pointer
	*   or multi-touch events, even if this window has input focus.
	*   This mask can't be used with #SCREEN_SENSITIVITY_MASK_STOP.
	*/
	SCREEN_SENSITIVITY_MASK_CONTINUE = (1 << 5),

	/**
	*   The window will be the final receiver of the pointer
	*   or multi-touch events.
	*   This mask can't be used with #SCREEN_SENSITIVITY_MASK_CONTINUE.
	*/
	SCREEN_SENSITIVITY_MASK_STOP = (2 << 5),

	/**
	*   The window receives pointer events, even in areas of transparency, when
	*   the source coordinates of the event are within the brush clip
	*   rectangle. This mode supersedes @c #SCREEN_SENSITIVITY_MASK_NEVER. The
	*   windowing system also draws brush strokes based on the pointer events
	*   directly onto the display and the window buffer.
	*/
	SCREEN_SENSITIVITY_MASK_POINTER_BRUSH = (1 << 7),

	/**
	*   The window receives multi-touch events with a finger contact type, even
	*   in areas of transparency, when the source coordinates of the event are
	*   within the brush clip rectangle. This mode supersedes
	*   @c #SCREEN_SENSITIVITY_MASK_NEVER. Screen also draws brush
	*   strokes based on the touch events directly onto the screen and the
	*   window buffer. Multiple contacts cause multiple brush strokes to
	*   be drawn.
	*/
	SCREEN_SENSITIVITY_MASK_FINGER_BRUSH = (1 << 8),

	/**
	*   The window receives multi-touch events with a stylus contact type, even
	*   in areas of transparency, when the source coordinates of the event are
	*   within the brush clip rectangle. This mode supersedes
	*   @c #SCREEN_SENSITIVITY_MASK_NEVER. Screen also draws brush
	*   strokes based on the touch events directly onto the screen and the
	*   window buffer. Multiple contacts cause multiple brush strokes to
	*   be drawn.
	*/
	SCREEN_SENSITIVITY_MASK_STYLUS_BRUSH = (1 << 9),

	/**
	 *   Setting this bit causes the system to go into overdrive when the window
	 *   gets an input event. The effect of this sensitivity mask depends on the
	 *   power management algorithms in place and on the platform in general.
	 */
	SCREEN_SENSITIVITY_MASK_OVERDRIVE = (1 << 10),

	/**
	 *   The window receives touch and point data only when the position is
	 *   within the bounds of the window. If a pointer drag operation or a
	 *   touch and move sequence leaves the window, the window receives
	 *   an input control notification that the event has left the bounds of
	 *   the window. The window won't receive subsequent events until the next
	 *   touch or button press, or until the events enter the window again.
	 *   Note that re-entry only occurs if the event leaves a window that
	 *   also has clipping enabled on its sensitivity mask.
	 */
	SCREEN_SENSITIVITY_MASK_CLIPPED = (1 << 11),
};

/**  @brief Types of scaling qualities
*
*   Each enumerator specifies the suggested amount of filtering to be performed
*   by Screen when scaling is required to draw the window. This
*   amount of filtering isn't a constant quantity; it's specified relative to each of the other
*   possible scale qualities.
*   @anonenum Screen_Scale_Quality_Types Screen scale quality types
*/
enum {
	/** The suggested amount of filtering that's slower than
	*   @c SCALE_QUALITY_FASTEST, but should have better quality.
	*/
	SCREEN_QUALITY_NORMAL = 0,

	/** The suggested amount of filtering that's faster than
	*   @c SCALE_QUALITY_NORMAL, but may have reduced quality.
	*/
	SCREEN_QUALITY_FASTEST = 1,

	/** The suggested amount of filtering that's slower than
	*   @c SCALE_QUALITY_NORMAL, but should have better quality.
	*/
	SCREEN_QUALITY_NICEST = 2,
};

/** @brief Types of content modes
 *
 *   The CBABC (content-based automatic brightness control) refers to the
 *   brightness control that's based on content, not ambient light. However,
 *   this enumeration is used mainly to describe the content type of the window,
 *   rather than the brightness control. If not set, the type defaults to
 *   the mode of the display framebuffer.
 *  @anonenum Screen_CBABC_Mode_Types Screen content mode types
 */
enum {
	/** The window content is not video, UI or photo. */
	SCREEN_CBABC_MODE_NONE = 0x7671,

	/** The window content is video. */
	SCREEN_CBABC_MODE_VIDEO = 0x7672,

	/** The window content is UI. */
	SCREEN_CBABC_MODE_UI = 0x7673,

	/** The window content is a photographic image. */
	SCREEN_CBABC_MODE_PHOTO = 0x7674,
};

/**
*   @addtogroup screen_windows
*   @{
*/
/** @brief Types of permission masks
*
*    These masks are intended to be combined in a single integer bitmask
*    representing combinations of desired permissions to be applied to a
*    window.
*    @anonenum Screen_Permission_Masks Screen permission masks
*/
enum {
	/** Read access to the owner of a window.*/
	SCREEN_PERMISSION_IRWIN = (1 << 0),

	/** Write access to the owner of a window.*/
	SCREEN_PERMISSION_IWWIN = (1 << 1),

	/** Inject access to the owner of a window.*/
	SCREEN_PERMISSION_IXWIN = (1 << 2),

	/** Read access to the parent of a window.*/
	SCREEN_PERMISSION_IRTOP = (1 << 4),

	/** Write access to the parent of a window.*/
	SCREEN_PERMISSION_IWTOP = (1 << 5),

	/** Inject access to the parent of a window.*/
	SCREEN_PERMISSION_IXTOP = (1 << 6),

	/** Read access to contexts with the same process id.*/
	SCREEN_PERMISSION_IRPID = (1 << 8),

	/** Write access to contexts with the same process id.*/
	SCREEN_PERMISSION_IWPID = (1 << 9),

	/** Inject access to contexts with the same process id.*/
	SCREEN_PERMISSION_IXPID = (1 << 10),

	/** Read access to others with the same process group id.*/
	SCREEN_PERMISSION_IRPGP = (1 << 12),

	/** Write access to others with the same process group id.*/
	SCREEN_PERMISSION_IWPGP = (1 << 13),

	/** Inject access to others with the same process group id.*/
	SCREEN_PERMISSION_IXPGP = (1 << 14),

	/** Read access to clients with the same user id.*/
	SCREEN_PERMISSION_IRUSR = (1 << 16),

	/** Write access to clients with the same user id.*/
	SCREEN_PERMISSION_IWUSR = (1 << 17),

	/** Inject access to clients with the same user id.*/
	SCREEN_PERMISSION_IXUSR = (1 << 18),

	/** Read access to clients with the same group id.*/
	SCREEN_PERMISSION_IRGRP = (1 << 20),

	/** Write access to clients with the same group id.*/
	SCREEN_PERMISSION_IWGRP = (1 << 21),

	/** Inject access to clients with the same group id.*/
	SCREEN_PERMISSION_IXGRP = (1 << 22),

	/** Read access to others in the same application group.*/
	SCREEN_PERMISSION_IRAGP = (1 << 24),

	/** Write access to others in the same application group.*/
	SCREEN_PERMISSION_IWAGP = (1 << 25),

	/** Inject access to others in the same application group.*/
	SCREEN_PERMISSION_IXAGP = (1 << 26),

	/** Read access to all others.*/
	SCREEN_PERMISSION_IROTH = (1 << 28),

	/** Write access to all others.*/
	SCREEN_PERMISSION_IWOTH = (1 << 29),

	/** Inject access to all others.*/
	SCREEN_PERMISSION_IXOTH = (1 << 30),
};
/** @} */   /* end of addtogroup screen_windows */

/**
*   @addtogroup screen_events
*   @{
*/
/** @brief Types of events
 *  @anonenum Screen_Event_Types Screen event types
 */
enum {
	/** A blocking event indicating that there are currently no events in the
	*   queue.
	*/
	SCREEN_EVENT_NONE = 0,

	/** Dispatched when:
	*   - a window or session is created
	*   - permissions to an object are gained
	*   - a window joins a group, or window group
	*
	*   Screen may locally allocate resources to track the object that's associated with this event when you retrieve
	*   an event of this type by calling screen_get_event(). When the object that's associated with this event is one
	*   of the following, Screen allocates local resources:
	*   - group
	*   - pixmap
	*   - session
	*   - stream
	*   - window
	*/
	SCREEN_EVENT_CREATE = 1,

	/** Dispatched when a property is set.*/
	SCREEN_EVENT_PROPERTY = 2,

	/** Dispatched when:
	*   - a window or session is destroyed
	*   - permissions to an object are lost
	*   - a window leaves a group, or window group
	*
	*   Screen frees local resources, if any, that it used track the object that's associated with this event when you
	*   retrieve an event of this type by calling screen_get_event(). When the object that's associated with this event
	*   is one of the following, Screen frees the local resources:
	*   - group
	*   - pixmap
	*   - session
	*   - stream
	*   - window
	*
	*   However, if at one time in your context, you acquire a handle to the object that's associated with this event
	*   (e.g., by calling screen_get_*_property_pv()), Screen doesn't free the local resources. You must call the
	*   appropriate screen_destroy_*() function to free these resources when you no longer need to use the object in
	*   your local context.
	*/
	SCREEN_EVENT_CLOSE = 3,

	/** Dispatched when an unknown input event occurs.*/
	SCREEN_EVENT_INPUT = 4,

	/** Used to describe various Screen API objects:
	*   - device: represents a device with a jog input, such as a jog dial or jog wheel
	*   - event: dispatched when a jog input event occurs, such as from a jog dial or jog wheel
	*   - session: a session that receives input from a device with jog input, such as a jog dial or jog wheel
	*/
	SCREEN_EVENT_JOG = 5,

	/** Used to describe various Screen API objects:
	*   - device: represents a pointer device
	*   - event: dispatched when a pointer input event occurs
	*   - session: a session that receives input from a pointer device
	*/
	SCREEN_EVENT_POINTER = 6,

	/** Used to describe various Screen API objects:
	*   - device: represents a keyboard
	*   - event: dispatched when a keyboard input event occurs
	*   - session: a session that receives input from a keyboard
	*/
	SCREEN_EVENT_KEYBOARD = 7,

	/** Dispatched when a user event is detected.*/
	SCREEN_EVENT_USER = 8,

	/** Dispatched when you call screen_post_window() or screen_post_stream()
	*   for a window or stream respectively for the first time. */
	SCREEN_EVENT_POST = 9,

	/** Dispatched when an external display is detected.*/
	SCREEN_EVENT_DISPLAY = 11,

	/** Dispatched when the window enters an idle state.*/
	SCREEN_EVENT_IDLE = 12,

	/** Dispatched when a handle to a window is lost.*/
	SCREEN_EVENT_UNREALIZE = 13,

	/** Used to describe various Screen API objects:
	*   - device: represents a gamepad
	*   - event: dispatched when a gamepad input event occurs
	*   - session: a session that receives input from a gamepad
	*/
	SCREEN_EVENT_GAMEPAD = 14,

	/** Used to describe various Screen API objects:
	*   - device: represents a joystick
	*   - event: dispatched when a joystick input event occurs
	*   - session: a session that receives input from a joystick
	*/
	SCREEN_EVENT_JOYSTICK = 15,

	/** Deprecated, will be removed in a later release. Dispatched when an input
	*   device is detected. @c #SCREEN_EVENT_CREATE and @c #SCREEN_EVENT_CLOSE
	*   should instead be used to determine a device's attached state.
	*/
	SCREEN_EVENT_DEVICE = 16,

	/** Dispatched when changes to the input state require that a
	*   notification be sent to the application for the event processing to
	*   be performed correctly. This includes gestures that may start
	*   without a touch or button press; or end without a release.
	*/
	SCREEN_EVENT_INPUT_CONTROL = 17,

	/** Used to describe a device or event API object:
	*   - device: represents a gesture-based input event occurs or when
	*           gesture recognition is enabled on a touch or pointer device
	*   - event: dispatched when a gesture-based input event occurs or when
	*           gesture recognition is enabled on a touch or pointer device
	*   - session: represents a valid input session type; used for a session
	*             object's @c #SCREEN_PROPERTY_TYPE
	*/
	SCREEN_EVENT_GESTURE = 18,

	/** Used for communication directly between applications and the specified manager.
	 *  This event is dispatched through use of @c screen_inject_event().
	 *  The @c screen_display parameter should be set to 0, and the
	 *  @c #SCREEN_PROPERTY_WINDOW of the event @e must be set to either the source
	 *  window (in the case of a message to the manager,) or the destination
	 *  window (in the case of a message from the manager).
	 *  This event is never dispatched directly by Screen. If the manager isn't available,
	 *  the error @c ENOSYS is returned.
	 */
	SCREEN_EVENT_MANAGER = 19,

	/** Dispatched when a multi-touch event is detected before it can be
	 *  properly resolved. This event is used to help reduce latency when touch
	 *  processing requires several samples before a touch event can be
	 *  formulated. Any handling of delayed touch events should keep in mind
	 *  that the event is very likely to be aborted by an input control
	 *  event.
	 */
	SCREEN_EVENT_MTOUCH_PRETOUCH = 99,

	/** Used to describe various Screen API objects:
	*   - device: represents a multi-touch device
	*   - event: dispatched when a multi-touch event is detected
	*   - session: a session that receives input from a multi-touch device
	*/
	SCREEN_EVENT_MTOUCH_TOUCH = 100,

	/** Dispatched when a multi-touch move event is detected. For example,
	 *  when the user moves his or her fingers to make an input gesture.
	 */
	SCREEN_EVENT_MTOUCH_MOVE = 101,

	/** Dispatched when a multi-touch release event occurs, or when the user
	 *  completes the multi-touch gesture.
	 */
	SCREEN_EVENT_MTOUCH_RELEASE = 102,
};

/** @brief Types of event flags
*
*    Flags that indicate whether the event contains logical data that can be
*    used by applications.
*    @anonenum Screen_Flags_Masks Screen event flags
*/
enum {
	/** Event contains keyboard mapping for key down. */
	SCREEN_FLAG_KEY_DOWN              = (1 << 0),

	/** Event contains keyboard mapping for key repeat. */
	SCREEN_FLAG_KEY_REPEAT            = (1 << 1),

	/** Event contains valid scan codes. */
	SCREEN_FLAG_SCAN_VALID            = (1 << 5),

	/** Event contains keyboard mapping for key symbols. */
	SCREEN_FLAG_SYM_VALID             = (1 << 6),

	/** Event contains keyboard mapping for caps lock. */
	SCREEN_FLAG_CAP_VALID             = (1 << 7),

	/** Event contains valid displacements. */
	SCREEN_FLAG_DISPLACEMENT_VALID    = (1 << 12),

	/** Event contains valid absolute display coordinates. */
	SCREEN_FLAG_POSITION_VALID        = (1 << 13),

	/** Event contains valid buffer coordinates. */
	SCREEN_FLAG_SOURCE_POSITION_VALID = (1 << 14),

	/** Event contains valid size in display coordinates. */
	SCREEN_FLAG_SIZE_VALID            = (1 << 15),
};
/** @} */   /* end of addtogroup screen_events */

/**
*   @addtogroup screen_blits
*   @{
*/
/** @brief Types of blit attributes
 *
 *  Note that support for each of the blit attributes is blitter-dependent. Different blitters have
 *  different capabilities and may even vary in how they each support these blit attributes.
 *
 *  @anonenum Screen_Blit_Types Screen blit types
 */
enum {
	/**  Used to terminate the token-value pairs in an attribute list. */
	SCREEN_BLIT_END = 0,

	/**
	*    The horizontal position of the rectangle in the source buffer.
	*    The offset is the distance, in pixels, from the left edge of the source
	*    buffer. If this attribute isn't specified, then a default of @c 0 is used.
	*/
	SCREEN_BLIT_SOURCE_X = 1,

	/**
	*    The vertical position of the rectangle in the source buffer.
	*    The offset is the distance, in pixels, from the top edge of the source
	*    buffer. If this attribute isn't specified, then a default of @c 0 is used.
	*/
	SCREEN_BLIT_SOURCE_Y = 2,

	/**
	*    The width, in pixels, of the rectangle in the source buffer.
	*    If this attribute isn't specified, then the source buffer width is used.
	*    The horizontal and vertical scale factors don't have to be
	*    equal. It's acceptable to specify a source width that's larger than
	*    the destination width while the source height is smaller than the
	*    destination height, and vice versa.
	*/
	SCREEN_BLIT_SOURCE_WIDTH = 3,

	/**
	*    The height, in pixels, of the rectangle in the source buffer.
	*    If this attribute isn't specified, then the source buffer height is used. The horizontal
	*    and vertical scale factors don't have to be equal. It is acceptable to specify a source
	*    width that is larger than the destination width while the source height is smaller than
	*    the destination height, and vice versa.
	*/
	SCREEN_BLIT_SOURCE_HEIGHT = 4,

	/**
	*    The horizontal position of the rectangle in the destination
	*    buffer. The offset is the distance, in pixels, from the left edge of the
	*    destination buffer. If this attribute isn't specified, then a default of @c 0 is used.
	*/
	SCREEN_BLIT_DESTINATION_X = 5,

	/**
	*    The vertical position of the rectangle in the destination
	*    buffer. The offset is the distance, in pixels, from the top edge of the
	*    destination buffer.  If this attribute isn't specified, then a default of @c 0 is used.
	*/
	SCREEN_BLIT_DESTINATION_Y = 6,

	/**
	*    The width, in pixels, of the rectangle in the destination buffer.
	*    The width doesn't have to match the source width. If the destination width is larger,
	*    the source rectangle is stretched. If the destination width is smaller than the source
	*    width, the source rectangle is downscaled. If this attribute isn't specified, then the
	*    destination buffer width is used.
	*/
	SCREEN_BLIT_DESTINATION_WIDTH = 7,

	/**
	*    The height, in pixels, of the rectangle in the destination buffer.
	*    The height doesn't have to match the source height. If the destination height is larger,
	*    the source rectangle is stretched. If the destination height is smaller than the source
	*    height, the source rectangle is downscaled. If this attribute isn't specified, then the
	*    destination buffer height is used.
	*/
	SCREEN_BLIT_DESTINATION_HEIGHT = 8,

	/**
	*    A global transparency value that's used to blend the source
	*    onto the destination. If this attribute isn't specified, then a default of @c 255 is used;
	*    this default indicates that no global transparency is applied to the source.
	*/
	SCREEN_BLIT_GLOBAL_ALPHA = 9,

	/**
	*    A transparency operation. The transparency setting defines how
	*    the alpha channel, if present, is used to combine the source and
	*    destination pixels. The transparency values must be of type
	*    <a href="screen_8h_1Screen_Transparency_Types.xml">Screen transparency types</a>.
	*    If this attribute isn't specified, then a default of @c #SCREEN_TRANSPARENCY_SOURCE is used.
	*/
	SCREEN_BLIT_TRANSPARENCY = 10,

	/**
	*    A scale quality value. The scale quality setting defines the
	*    type and amount of filtering applied when scaling is required. If the
	*    source and destination rectangles are identical in size, the scale
	*    quality setting isn't used. The scale quality value must be of type
	*    <a href="screen_8h_1Screen_Scale_Quality_Types.xml">Screen scale quality types</a>.
	*    If this attribute isn't specified, then a default of @c #SCREEN_QUALITY_NORMAL is used.
	*/
	SCREEN_BLIT_SCALE_QUALITY = 11,

	/**
	*    The color that's used by @c screen_fill(). The color format is alpha value in bits 24 to 31,
	*    red in bits 16 to 23, green in bits 8 to 15 and blue in bits 0 to 7.
	*    If this attribute isn't specified, then a default of @c \#00000000 (transparent) is used.
	*/
	SCREEN_BLIT_COLOR = 12,

	/**
	*    The rotation that screen_blit() supports (screen_fill() doesn't support this blit attribute). The
	*    values must be of type <a href="screen_8h_1Screen_Rotation_Types.xml">Screen rotation types</a>.
	*    If this attribute isn't specified, then a default of @c #SCREEN_ROTATION_NONE is used.
	*/
	SCREEN_BLIT_ROTATION = 13,
};
/** @} */


/**   @brief Types of flushing options
 *    @details These options aren't valid for the @c screen_acquire_buffer() function. If you are
 *             calling @c screen_acquire_buffer(), use
 *             <a href="screen_8h_1Screen_Acquire_Flags.xml">Screen acquire buffer flags</a> instead.
 *    @anonenum Screen_Flushing_Types Screen flushing types
 */
enum {
	/**
	*    Indicator that the function blocks until the operation has completed.
	*/
	SCREEN_WAIT_IDLE                       = (1 << 0),
	/** Internal use only; do not use. */
	SCREEN_PROTECTED                       = (1 << 1),
	/** Internal use only; do not use. Deprecated, will be removed in a later release */
	SCREEN_DONT_FLUSH = (1 << 2),
	/** Internal use only; do not use. Deprecated, will be removed in a later release */
	SCREEN_POST_RESUME = (1 << 3),
	/** Internal use only; do not use. */
	SCREEN_POST_RESIZE                     = (1 << 4),
	/** Indicator that the function returns immediately. */
	SCREEN_DONT_BLOCK                      = (1 << 5),
	/** Reserved bit for future use; do not use */
	SCREEN_RESERVED_1                      = (1 << 6),
	/** Indicator that the function should leave the buffer dequeued as
	 *  @c screen_enqueue_render_buffer() will be used.
	 */
	SCREEN_EXPLICIT_ENQUEUE                = (1 << 7),

	SCREEN_INTERLACED_TOP                  = (1 << 30), /* internal use only */
	SCREEN_INTERLACED_BOTTOM               = (1 << 31), /* internal use only */
};

enum {
	SCREEN_INTERLACED_NONE                 = 0, /* internal use only */
	SCREEN_INTERLACED_TOP_FIELD            = 1, /* internal use only */
	SCREEN_INTERLACED_BOTTOM_FIELD         = 2, /* internal use only */
};

#define SCREEN_INTERLACED_TOP_FIELD_FIRST    SCREEN_INTERLACED_TOP_FIELD
#define SCREEN_INTERLACED_BOTTOM_FIELD_FIRST SCREEN_INTERLACED_BOTTOM_FIELD

/** @brief Options for the @c flags parameter when you try to acquire a buffer from a producer
 *  @details These options are valid only for the @c screen_acquire_buffer() function. Use
 *           <a href="screen_8h_1Screen_Flushing_Types.xml">Screen flushing types</a> for other Screen API functions
 *           that accept a @c flags parameter.
 *
 *  @anonenum Screen_Acquire_Flags Screen acquire buffer flags
 */
enum {
	/** Atomically release all other buffers when a new one is acquired.
	*   This flag is associated with the preprocessor definition @c SCREEN_ACQUIRE_AND_RELEASE_OTHERS.
	*   When you use this flag, you must also specify @c #SCREEN_ACQUIRE_DONT_BLOCK.
	*/
	SCREEN_ACQUIRE_AND_RELEASE_OTHERS = (1 << 0),

	/** Return immediately if no buffer is available.
	*   This flag is associated with the preprocessor definition @c SCREEN_ACQUIRE_DONT_BLOCK.
	*/
	SCREEN_ACQUIRE_DONT_BLOCK = (1 << 5),
};

/** @brief Types of mouse buttons
 *  @anonenum Screen_Mouse_Button_Types Screen mouse button types
 */
enum {
	SCREEN_LEFT_MOUSE_BUTTON               = (1 << 0),  /**< Left mouse button. */
	SCREEN_MIDDLE_MOUSE_BUTTON             = (1 << 1),  /**< Middle mouse button. */
	SCREEN_RIGHT_MOUSE_BUTTON              = (1 << 2),  /**< Right mouse button. */
};

/** @brief Types of stylus buttons
 *  @anonenum Screen_Stylus_Button_Types Screen stylus button types
 */
enum {
	SCREEN_LOWER_STYLUS_BUTTON             = (1 << 0), /**< Lower stylus button. */
	SCREEN_UPPER_STYLUS_BUTTON             = (1 << 1), /**< Upper stylus button. */
};

/**  @brief Types of touch
 *   @anonenum Screen_Touch_Types Screen touch types
 */
enum {
	SCREEN_TOUCH_FINGER                    = 0, /**< Contact made by a finger. */
	SCREEN_TOUCH_STYLUS                    = 1, /**< Contact made by a stylus. */
};

/**  @brief Types of gestures
 *   @anonenum Screen_Gesture_Types Screen gesture types
 */
enum {
	/** A gesture comprised of one touch with one point of contact.	*/
	SCREEN_GESTURE_TAP                     = 0x2C,
	/** A gesture that's one continuous motion, comprised of a two-contact tap, a movement
	*   inward, and a release.
	*/
	SCREEN_GESTURE_ZOOM_OUT                = 0x2D,
	/** A gesture that's one continuous motion, comprised of a two-contact tap, a movement
	*   outward, and a release.
	*/
	SCREEN_GESTURE_ZOOM_IN                 = 0x2E,
	/** A gesture that's one continuous horizontal motion, comprised of a tap, a movement towards
	*   the right, and a release, all with one contact.
	*/
	SCREEN_GESTURE_SWIPE_RIGHT             = 0x4F,
	/** A gesture that's one continuous horizontal motion, comprised of a tap, a movement towards
	*   the left, and a release, all with one contact.
	*/
	SCREEN_GESTURE_SWIPE_LEFT              = 0x50,
	/** A gesture that's one continuous vertical motion, comprised of a tap, a movement downwards,
	*   and a release, all with one contact.
	*/
	SCREEN_GESTURE_SWIPE_DOWN              = 0x51,
	/** A gesture that's one continuous vertical motion, comprised of a tap, a movement upwards,
	*   all with one contact.
	*/
	SCREEN_GESTURE_SWIPE_UP                = 0x52,
	/** A gesture that's one touch with a pause before releasing. */
	SCREEN_GESTURE_HOLD                    = 0xE1,
};

/**
*   @addtogroup screen_devices
*   @{
*/
/** @brief Types of game buttons
 *
 *  These enumerator values are used as constants to map buttons from
 *  different controllers to a common game control layout. Typically, you create
 *  a structure to represent your game controller and map the buttons to
 *  constants in this enumeration.
 *
 *  @anonenum Screen_Game_Button_Types Screen game button types
 */
enum {
	SCREEN_A_GAME_BUTTON                   = (1 << 0),	/**< Button A on a game controller. */
	SCREEN_B_GAME_BUTTON                   = (1 << 1),	/**< Button B on a game controller. */
	SCREEN_C_GAME_BUTTON                   = (1 << 2),	/**< Button C on a game controller. */
	SCREEN_X_GAME_BUTTON                   = (1 << 3),	/**< Button X on a game controller. */
	SCREEN_Y_GAME_BUTTON                   = (1 << 4),	/**< Button Y on a game controller. */
	SCREEN_Z_GAME_BUTTON                   = (1 << 5),	/**< Button Z on a game controller. */
	SCREEN_MENU1_GAME_BUTTON               = (1 << 6),	/**< Menu 1 button on a game controller. */
	SCREEN_MENU2_GAME_BUTTON               = (1 << 7),	/**< Menu 2 button on a game controller. */
	SCREEN_MENU3_GAME_BUTTON               = (1 << 8),	/**< Menu 3 button on a game controller. */
	SCREEN_MENU4_GAME_BUTTON               = (1 << 9),	/**< Menu 4 button on a game controller. */
	SCREEN_L1_GAME_BUTTON                  = (1 << 10),	/**< L1 button on a game controller. */
	SCREEN_L2_GAME_BUTTON                  = (1 << 11),	/**< L2 button on a game controller. */
	SCREEN_L3_GAME_BUTTON                  = (1 << 12),	/**< L3 button on a game controller. */
	SCREEN_R1_GAME_BUTTON                  = (1 << 13),	/**< R1 button on a game controller. */
	SCREEN_R2_GAME_BUTTON                  = (1 << 14),	/**< R2 button on a game controller. */
	SCREEN_R3_GAME_BUTTON                  = (1 << 15),	/**< R3 button on a game controller. */
	SCREEN_DPAD_UP_GAME_BUTTON             = (1 << 16),	/**< Up direction of the directional pad on a game controller.*/
	SCREEN_DPAD_DOWN_GAME_BUTTON           = (1 << 17),	/**< Down direction of the directional pad on a game controller. */
	SCREEN_DPAD_LEFT_GAME_BUTTON           = (1 << 18),	/**< Left direction of the directional pad on a game controller. */
	SCREEN_DPAD_RIGHT_GAME_BUTTON          = (1 << 19),	/**< Right direction of the directional pad on a game controller. */
};
/** @} */   /* end of addtogroup screen_devices */

/**
*   @addtogroup screen_events
*   @{
*/
/**  @brief Types of input control events
 *   @anonenum Screen_Input_Control_Events Screen input control events
 */
enum {
	/**
	*   A touch event that started outside the window, but is re-focused
	*   to this window.
	*/
	SCREEN_INPUT_CONTROL_MTOUCH_START = 0,

	/**
	*   A touch event that started inside the window but won't end
	*   because it's re-focused to another window.
	*/
	SCREEN_INPUT_CONTROL_MTOUCH_STOP = 1,

	/**
	*   A pointer event that started outside the window, but is re-focused to this window. This
	*   isn't sent unless one of the mouse buttons is pressed when the sequence switched
	*   focus to another window.
	*/
	SCREEN_INPUT_CONTROL_POINTER_START = 2,

	/**
	 *   A pointer event that started inside the window, but won't end
	 *   in this window because it's re-focused to another window.
	 */
	SCREEN_INPUT_CONTROL_POINTER_STOP = 3,
};
/** @} */   /* end of addtogroup screen_events */

/**
*   @addtogroup screen_contexts
*   @{
*/
/** @brief Types of notifications
 *  @anonenum Screen_Notification_Types Screen notification types
 */
enum {
	/** Not supported; do not use. */
	SCREEN_NOTIFY_VSYNC                    = 0,

	/** Notification of an update; this notification type is applicable to the following API objects:
	*   - display: notification when Screen completes composition
	*   - stream: notification when @c screen_post_stream() has been called with the specified stream
	*   - window: notification when @c screen_post_window() has been called with the specified window
	*
	*   When you register for @c SCREEN_NOTIFY_UPDATE notifications, Screen still adds
	*   @c #SCREEN_EVENT_POST events to your context's event queue when applicable.
	*/
	SCREEN_NOTIFY_UPDATE                   = 1,

	/** Notification of an event from a display or an input device; this notification type is
	*   applicable to the following API objects:
	*   - device: notification when the specified device is sending input;
	*             note that when you receive a notification for input, it isn't necessarily an
	*             indication that there's an event corresponding to your input device in your
	*             context's event queue.
	*   - display: notification when there's input occurring that's targeted for the specified display
	*
	*   When you register for @c SCREEN_NOTIFY_INPUT notifications with a NULL object, then
	*   you receive notifications when @e any input device is sending input
	*/
	SCREEN_NOTIFY_INPUT                    = 2,

	/** Notification of when a property has been changed on the specified API object or when an
	*   event is available in your context's event queue; this notification type is applicable to
	*   the following API objects:
	*   - device
	*   - group
	*   - pixmap
	*   - session
	*   - stream
	*   - window
	*
	*   When you register for @c SCREEN_NOTIFY_EVENT notifications for any of the above API objects,
	*   Screen sends a notification when a property has been changed on that specified object.
	*   If the property change also invokes Screen to send a @c #SCREEN_EVENT_PROPERTY event,
	*   then you will receive this event in your context's event queue in addition to the
	*   notification; otherwise, you receive only the notification. Refer to
	*   <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a> to determine
	*   whether a property sends a change event.
	*
	*   When you register for @c SCREEN_NOTIFY_EVENT notifications with a NULL object, then
	*   you receive one notification for each event that's residing in your context's event
	*   queue (i.e., one notification for each event that you can call @c screen_get_event() to retrieve).
	*/
	SCREEN_NOTIFY_EVENT                    = 3,

	/** Notification of display update; this notification type is applicable to
	 *  display API objects
	 */
	SCREEN_NOTIFY_DISPLAY                   = 4,
};
/** @} */   /* end of addtogroup screen_contexts */

/**
*   @addtogroup screen_debugging
*   @{
*/
/** @brief Types of debug graphs
*
*   All masks except @c #SCREEN_DEBUG_STATISTICS are intended to be
*   combined in a single integer bitmask. The bitmask represents combinations
*   of desired debug graphs to be displayed. Only one window can enable debug
*   graphs at a time; the last window to have enabled debug will have its
*   values displayed in the graph. All data but the FPS is normalized to buffer
*   size and refresh rate of display.
*   @anonenum Screen_Debug_Graph_Types Screen debug graph types
*/
enum {
	/** Frames per second; the number of posts over time */
	SCREEN_DEBUG_GRAPH_FPS = (1 << 0),

	/** Pixel count of pixels in dirty rectangles over time */
	SCREEN_DEBUG_GRAPH_POSTS = (1 << 1),

	/** Pixel count of pixels that were in blit requests over time */
	SCREEN_DEBUG_GRAPH_BLITS = (1 << 2),

	/** Pixel count of pixels used by Screen in the window
	*   to update the framebuffer over time	*/
	SCREEN_DEBUG_GRAPH_UPDATES = (1 << 3),

	/** The time spent on the CPU drawing each frame */
	SCREEN_DEBUG_GRAPH_CPU_TIME = (1 << 4),

	/** The time spent on the GPU drawing each frame */
	SCREEN_DEBUG_GRAPH_GPU_TIME = (1 << 5),

	/** Certain statistics of a window. The statistics are updated once per
	*  second and therefore represent a one second average. The following statistics
	* are displayed:
	*   - cpu usage, cpu time, gpu time
	*   - private mappings, free memory
	*   - window fps, display fps
	*   - events
	*   - objects
	*   - draws
	*   - triangles
	*   - vertices
	*/
	SCREEN_DEBUG_STATISTICS = (1 << 7),
};
/** @} */   /* end of addtogroup screen_debugging */

/**
*   @addtogroup screen_devices
*   @{
*/
/** @brief Types of metric counts for devices
*
*   The metrics are on a per device basis and the counts are reset after being
*   queried. That is, the counts are reset to @c 0 after you call
*   @c screen_get_device_property_llv() to retrieve @c #SCREEN_PROPERTY_METRICS.
*   @anonenum Screen_Device_Metric_Count_Types Screen device metric count types
*/
enum {
	/** The number of input events generated by the device since the last time
	*   Screen device metric count types were queried.
	*/
	SCREEN_DEVICE_METRIC_EVENT_COUNT = 0,

	/** The number of times that the device has been powered on since the last
	*   time Screen device metrics were queried.
	*/
	SCREEN_DEVICE_METRIC_POWER_ON_COUNT = 1,

	/** The number of times the devices has gone idle since the last time
	 *  Screen device metrics were queried.
	 */
	SCREEN_DEVICE_METRIC_IDLE_COUNT = 2,
};
/** @} */   /* end of addtogroup screen_devices */

/**
*   @addtogroup screen_displays
*   @{
*/
/** @brief Types of metric counts for displays
*
*   The metrics are on a per display basis and the counts are reset after being
*   queried. That is, the counts are reset to @c 0 after you call
*   @c screen get_display_property_llv() to retrieve @c #SCREEN_PROPERTY_METRICS.
*   @anonenum Screen_Display_Metric_Count_Types Screen display metric count types
*/
enum {
	/** The number of times the display has been attached (connected) since
	*   the last time Screen display metrics were queried.
	*/
	SCREEN_DISPLAY_METRIC_ATTACH_COUNT = 0,

	/** The number of times the display has been powered on since
	*   the last time Screen display metrics were queried.
	*/
	SCREEN_DISPLAY_METRIC_POWER_ON_COUNT = 1,

	/** The number of times the display has been in idle state since
	*   the last time Screen display metrics were queried.
	*/
	SCREEN_DISPLAY_METRIC_IDLE_COUNT = 2,

	/** The number of input events that has been focused (or sent) to the
	*    display since the last time Screen display metrics were queried.
	*/
	SCREEN_DISPLAY_METRIC_EVENT_COUNT = 3,

	/** The number of times that the framebuffer of the display has been
	*    updated since the last time Screen display metrics were queried.
	*/
	SCREEN_DISPLAY_METRIC_UPDATE_COUNT = 4,

	/** The number of pixels that the framebuffer of the display has
	*    updated since the last time Screen display metrics were queried.
	*/
	SCREEN_DISPLAY_METRIC_UPDATE_PIXELS = 5,

	/** The number of bytes that has been read from the framebuffer
	*   of the display since the last time Screen display metrics have been
	*   queried. The number of bytes read is an estimation calculated based on
	*   the number of pixels updated by the framebuffer.
	*/
	SCREEN_DISPLAY_METRIC_UPDATE_READS = 6,

	/** The number of bytes that has been written to the framebuffer
	*   of the display since the last time Screen display metrics have been
	*   queried. The number of bytes written is an estimation calculated
	*   based on the number of pixels updated by the framebuffer.
	*/
	SCREEN_DISPLAY_METRIC_UPDATE_WRITES = 7,
};
/** @} */   /* end of addtogroup screen_displays */

/**
*   @addtogroup screen_pixmaps
*   @{
*/
/** @brief Types of metric counts for pixmaps
*
*   The metrics are on a per pixmap basis and the counts are reset after being
*   queried. That is, the counts are reset to @c 0 after you call
*   @c screen_get_pixmap_property_llv() to retrieve @c #SCREEN_PROPERTY_METRICS.
*   @anonenum Screen_Pixmap_Metric_Count_Types Screen pixmap metric count types
*/
enum {
	/** The number of blit requests (when the pixmap was a target of a blit)
	*   since the last time Screen pixmap metrics were queried.
	*/
	SCREEN_PIXMAP_METRIC_BLIT_COUNT = 0,

	/** The number of pixels affected by the blit requests (when the pixmap
	*   was a target of a blit) since the last time Screen pixmap metrics have
	*   been queried.
	*/
	SCREEN_PIXMAP_METRIC_BLIT_PIXELS = 1,

	/** An estimate of the number of bytes that has been read from the pixmap
	*   since the last time Screen pixmap metrics were queried.
	*   The number of bytes read is an estimation calculated based on
	*   the number of pixels affected by the blit requests.
	*/
	SCREEN_PIXMAP_METRIC_BLIT_READS = 2,

	/** An estimate of the number of bytes that has been written to the pixmap
	*   since the last time Screen pixmap metrics were queried.
	*   The number of bytes written is an estimation calculated based on
	*   the number of pixels affected by the blit requests.
	*/
	SCREEN_PIXMAP_METRIC_BLIT_WRITES = 3,
};
/** @} */   /* end of addtogroup screen_pixmaps */

/**
*   @addtogroup screen_sessions
*   @{
*/
/** @brief Types of metric counts for sessions
*
*   The metrics are on a per session basis and the counts are reset after being
*   queried. That is, the counts are reset to @c 0 after you call
*   @c screen_get_session_property_llv() to retrieve @c #SCREEN_PROPERTY_METRICS.
*   @anonenum Screen_Session_Metric_Count_Types Screen session metric count types
*/
enum {
	/** The number of input events received by the session since the last time
	*    Screen session metric count types were queried.
	*/
	SCREEN_SESSION_METRIC_EVENT_COUNT = 0,
};

/** @brief Types of input modes for devices and sessions.
*   @anonenum Screen_Input_Mode_Types Screen input mode types
*/
enum {
	/** Used when you want minimal filtering. The only filtering is to focus the event, such as
	*   checking the (x,y) position to direct the event to the session. Applicable to the
	*   following device and session types:
	*   - @c #SCREEN_EVENT_GAMEPAD
	*   - @c #SCREEN_EVENT_JOG
	*   - @c #SCREEN_EVENT_JOYSTICK
	*   - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*/
	SCREEN_INPUT_MODE_RAW = 4,

	/** The displacement is converted into a pointer event. In this mode, all the pointer movements
	*   are clipped to the input region (defined by @c #SCREEN_PROPERTY_SIZE and
	*   @c #SCREEN_PROPERTY_POSITION of the session) of your session. It's not recommended that you
	*   use this mode if you have other pointer devices on the same system because the interactions
	*   can become quite complicated. Applicable to the following session types:
	*   - @c #SCREEN_EVENT_GAMEPAD
	*   - @c #SCREEN_EVENT_JOG
	*   - @c #SCREEN_EVENT_JOYSTICK
	*/
	SCREEN_INPUT_MODE_POINTER = 5,

	/** Used to handle touch events that would otherwise be ignored because they originate off-screen.
	*   Typically, you use this mode for a session that's not associated with a window, but has size
	*   and position that's off-screen. For example, a session with @c SCREEN_INPUT_MODE_BEZEL can
	*   be used to handle edge swipes. This mode is applicable only for the
	*   @c #SCREEN_EVENT_MTOUCH_TOUCH session type, and not applicable at all to devices.
	*/
	SCREEN_INPUT_MODE_BEZEL = 6,

	/** All input events are converted to @c #SCREEN_EVENT_GESTURE events. A displacement is
	*   generated, and this displacement is converted into a gesture. Applicable to the following
	*   device and session types:
	*   - @c #SCREEN_EVENT_GAMEPAD
	*   - @c #SCREEN_EVENT_JOG
	*   - @c #SCREEN_EVENT_JOYSTICK
	*   - @c #SCREEN_EVENT_MTOUCH_TOUCH
	*/
	SCREEN_INPUT_MODE_GESTURE = 7,
};
/** @} */   /* end of addtogroup screen_sessions */

/**
*   @addtogroup screen_streams
*   @{
*/
/** @brief Types of stream modes.
*   @anonenum Screen_Stream_Mode_Types Screen stream mode types
*/
enum {
	/** Screen decides whether to use @c #SCREEN_STREAM_MODE_FIFO or @c #SCREEN_STREAM_MODE_MAILBOX mode.
	*/
	SCREEN_STREAM_MODE_DEFAULT = 0,

	/** The mode that's recommended when timing is controlled by the consumer
	*   (e.g., typical rendering using native, OpenGL ES, or OpenVG ).
	*/
	SCREEN_STREAM_MODE_FIFO = 1,

	/** The mode that's recommended when timing is controlled by the producer
	*   (e.g., video playback, or video capture (camera) ).
	*/
	SCREEN_STREAM_MODE_MAILBOX = 2,
};

/** @brief Types of metric counts for streams
*
*   The metrics are on a per-stream basis, and the counts are reset after being
*   queried. That is, the counts are reset to @c 0 after you call
*   @c screen_get_stream_property_llv() to retrieve @c #SCREEN_PROPERTY_METRICS.
*   @anonenum Screen_Stream_Metric_Count_Types Screen stream metric count types
*/
enum {
	/** A general purpose counter whose meaning is defined by SDKs
	*   (e.g., WebKit, Adobe AIR, ...).
	*/
	SCREEN_STREAM_METRIC_OBJECT_COUNT = 0,

	/** The number of OpenGL ES 1.X, 2.X, or 3.X, and OpenVG API calls that
	*   were made by the process owning the stream since the last time Screen
	*   stream metrics were queried. If multiple processes, other
	*   than the one that owns the stream, made OpenGL ES 1.X, 2.X, 3.X, or
	*   OpenVG API calls to the stream, these API calls aren't counted.
	*/
	SCREEN_STREAM_METRIC_API_COUNT = 1,

	/** The number of draw API calls (e.g., @c glDrawArrays(),
	*   @c glDrawElements(), ...) that were made by in the stream since the last
	*   time Screen stream metrics were queried. This metric isn't counted for
	*   OpenVG API calls.
	*/
	SCREEN_STREAM_METRIC_DRAW_COUNT = 2,

	/** An estimate of the number of triangles drawn in the stream since the last
	*   time Screen stream metrics were queried. This count
	*   is an estimate because two triangles are counted per line, and two
	*   triangles are also counted per point. This metric isn't counted for OpenVG
	*   API calls.
	*/
	SCREEN_STREAM_METRIC_TRIANGLE_COUNT = 3,

	/** An estimate of the number of vertices passed to OpenGL in the stream since
	*   the last time Screen stream metrics were queried.
	*   This metric isn't counted for OpenVG API calls.
	*/
	SCREEN_STREAM_METRIC_VERTEX_COUNT = 4,

	/** An estimate of the number of bytes requested to upload the texture in
	*   the stream since the last time Screen stream metrics were
	*   queried. This metric isn't counted for OpenVG API calls.
	*/
	SCREEN_STREAM_METRIC_IMAGE_DATA_BYTES = 5,

	/** An estimate of the number of bytes uploaded to vertex buffers in
	*   the stream (e.g., from calls such as @c glBufferData(),
	*   @c glBufferSubData(), ...) since the last time Screen stream metrics
	*   were queried. This metric isn't counted for OpenVG API calls.
	*/
	SCREEN_STREAM_METRIC_BUFFER_DATA_BYTES = 6,

	/** The number of blit requests (when the stream was a target of a blit)
	*   since the last time Screen stream metrics were queried.
	*/
	SCREEN_STREAM_METRIC_BLIT_COUNT = 7,

	/** The number of pixels affected by the blit requests (when the stream
	*   was a target of a blit) since the last time Screen stream metrics were
	*   queried.
	*/
	SCREEN_STREAM_METRIC_BLIT_PIXELS = 8,

	/** An estimate of the number of bytes that have been read from the stream
	*   since the last time Screen stream metrics were queried.
	*   The number of bytes read is an estimate based on
	*   the number of pixels affected by the blit requests.
	*/
	SCREEN_STREAM_METRIC_BLIT_READS = 9,

	/** An estimate of the number of bytes that have been written to the stream
	*   since the last time Screen stream metrics were queried.
	*   The number of bytes written is an estimate based on
	*   the number of pixels affected by the blit requests.
	*/
	SCREEN_STREAM_METRIC_BLIT_WRITES = 10,

	/** The number times that the stream has posted since the last time
	*   Screen stream metrics were queried.
	*/
	SCREEN_STREAM_METRIC_POST_COUNT = 11,

	/** The number of pixels that were marked as dirty in all of the stream posts
	*   since the last time Screen stream metrics were queried.
	*/
	SCREEN_STREAM_METRIC_POST_PIXELS = 12,

	/** An estimate of the total CPU time (in microseconds) spent preparing updates.
	*   The quantity is estimated by measuring the time between the stream
	*   timestamp property and the time when @c screen_post_stream() is called.
	*   The @c #SCREEN_PROPERTY_TIMESTAMP must be set on the stream for this
	*   metric to be reliable.
	*/
	SCREEN_STREAM_METRIC_CPU_TIME = 13,

	/** An estimate of the total GPU time (in microseconds) spent rendering to
	*   back buffers. The quantity is estimated by measuring the time between when
	*   @c eglSwapBuffers() is called and when the post is actually flushed out
	*   to the server. This metric is reliable only if the GPU does most of its
	*   rendering after @c eglSwapBuffers() is called.
	*/
	SCREEN_STREAM_METRIC_GPU_TIME = 14,
};
/** @} */   /* end of addtogroup screen_streams */

/**
*   @addtogroup screen_windows
*   @{
*/
/** @brief Types of metric counts for windows
*
*   The metrics are on a per-window basis, and the counts are reset after being
*   queried. That is, the counts are reset to @c 0 after you call
*   @c screen_get_window_property_llv() to retrieve @c #SCREEN_PROPERTY_METRICS.
*   @anonenum Screen_Window_Metric_Count_Types Screen window metric count types
*/
enum {
	/** A general purpose counter whose meaning is defined by SDKs
	*   (e.g., WebKit, Adobe AIR, ...).
	*/
	SCREEN_WINDOW_METRIC_OBJECT_COUNT = 0,

	/** The number of OpenGL ES 1.X, 2.X, 3.X, and OpenVG API calls that
	*   were made by the process owning the window since the last time Screen
	*   window metrics were queried. Note that if multiple processes, other
	*   than the one that owns the window, made OpenGL ES 1.X, 2.X, 3.X,
	*   OpenVG API calls to the window, these API calls aren't counted.
	*/
	SCREEN_WINDOW_METRIC_API_COUNT = 1,

	/** The number of draw API calls (e.g., @c glDrawArrays(),
	*   @c glDrawElements(), ...) that were made by in the window since the last
	*   time Screen window metrics were queried. This metric isn't counted for
	*   OpenVG API calls.
	*/
	SCREEN_WINDOW_METRIC_DRAW_COUNT = 2,

	/** An estimate of the number of triangles drawn in the window since the last
	*   time Screen window metrics were queried. This count
	*   is an estimate because two triangles are counted per line and two
	*   triangles are also counted per point. This metric isn't counted for OpenVG
	*   API calls.
	*/
	SCREEN_WINDOW_METRIC_TRIANGLE_COUNT = 3,

	/** An estimate of the number of vertices passed to OpenGL in the window since
	*   the last time Screen window metrics were queried.
	*   This metric isn't counted for OpenVG API calls.
	*/
	SCREEN_WINDOW_METRIC_VERTEX_COUNT = 4,

	/** An estimate of the number of bytes requested to upload the texture in
	*   the window since the last time Screen window metrics were
	*   queried. This metric isn't counted for OpenVG API calls.
	*/
	SCREEN_WINDOW_METRIC_IMAGE_DATA_BYTES = 5,

	/** An estimate of the number of bytes uploaded to vertex buffers in
	*   the window (e.g., from calls such as @c glBufferData(),
	*   @c glBufferSubData(), ...) since the last time Screen window metrics
	*   were queried. This metric isn't counted for OpenVG API calls.
	*/
	SCREEN_WINDOW_METRIC_BUFFER_DATA_BYTES = 6,

	/** The number of events that are sent directly to the window since the last time
	*   Screen window metrics were queried. This metric doesn't include
	*   events for any child windows that the window may have.
	*/
	SCREEN_WINDOW_METRIC_EVENT_COUNT = 7,

	/** The number of blit requests (when the window was a target of a blit)
	*   since the last time Screen window metrics were queried.
	*/
	SCREEN_WINDOW_METRIC_BLIT_COUNT = 8,

	/** The number of pixels affected by the blit requests (when the window
	*   was a target of a blit) since the last time Screen window metrics have
	*   been queried.
	*/
	SCREEN_WINDOW_METRIC_BLIT_PIXELS = 9,

	/** An estimate of the number of bytes that have been read from the window
	*   since the last time Screen window metrics were queried.
	*   The number of bytes read is an estimation calculated based on
	*   the number of pixels affected by the blit requests.
	*/
	SCREEN_WINDOW_METRIC_BLIT_READS = 10,

	/** An estimate of the number of bytes that have been written to the window
	*   since the last time Screen window metrics were queried.
	*   The number of bytes written is an estimate based on
	*   the number of pixels affected by the blit requests.
	*/
	SCREEN_WINDOW_METRIC_BLIT_WRITES = 11,

	/** The number times that the window has posted since the last time
	*   Screen window metrics were queried.
	*/
	SCREEN_WINDOW_METRIC_POST_COUNT = 12,

	/** The number of pixels that were marked as dirty in all of the window's posts
	*   since the last time Screen window metrics were queried.
	*/
	SCREEN_WINDOW_METRIC_POST_PIXELS = 13,

	/** The number of times that the window was in an update since the last
	*   time Screen window metrics were queried. The window
	*   must be visible (its @c #SCREEN_PROPERTY_VISIBLE is set) in order
	*   for this count to be incremented. If the window is static (i.e., the window
	*   property @c #SCREEN_PROPERTY_STATIC is set), this count can still
	*   increment if there is another window or layer on top so that there is
	*   blending required for this window.
	*/
	SCREEN_WINDOW_METRIC_UPDATE_COUNT = 14,

	/** The number of pixels that has been used in the updates of the window
	*   since the last time Screen window metrics were queried.
	*/
	SCREEN_WINDOW_METRIC_UPDATE_PIXELS = 15,

	/** An estimate of the number of bytes that have been read from the
	*   window buffer (if there are multiple buffers, it's the front buffer)
	*   since the last time Screen window metrics were queried.
	*   The number of bytes read is an estimate based on
	*   the number of pixels affected by the update.
	*/
	SCREEN_WINDOW_METRIC_UPDATE_READS = 16,

	/** An estimate of the number of bytes that has been written to the
	*   window framebuffer since the last time Screen window metrics have
	*   been queried. The number of bytes written is an estimate
	*   based on the number of pixels affected by the update.
	*/
	SCREEN_WINDOW_METRIC_UPDATE_WRITES = 17,

	/** An estimate of the total CPU time (in microseconds) spent preparing updates.
	*   The quantity is estimated by measuring the time between the window
	*   timestamp property and the time when @c screen_post_window() is called.
	*   The @c #SCREEN_PROPERTY_TIMESTAMP must be set on the window for this
	*   metric to be reliable.
	*/
	SCREEN_WINDOW_METRIC_CPU_TIME = 18,

	/** An estimate of the total GPU time (in microseconds) spent rendering to
	*   back buffers. The quantity is estimated by measuring the time between when
	*   @c eglSwapBuffers() is called and when the post is actually flushed out
	*   to the server. This metric is only reliable if the GPU does most of its
	*   rendering after @c eglSwapBuffers() is called.
	*/
	SCREEN_WINDOW_METRIC_GPU_TIME = 19,

	/**  An estimate of the total number of nanoseconds for which the window
	 *  was visible. The quantity is estimated by measuring the time spent
	 *  between scene rebuilds where the window is at least partially visible.
	 *  If the window is covered by another window with transparency, the
	 *  counter is incremented.
	 */
	SCREEN_WINDOW_METRIC_VISIBLE_TIME = 20,

	/**  An estimate of the total number of nanoseconds for which the window
	 *  was fully visible. The quantity is estimated by measuring the time spent
	 *  between scene rebuilds where the window is completely visible. If the
	 *  window is covered by another window with transparency, the counter won't
	 *  be incremented even though the window may actually be visible.
	 */
	SCREEN_WINDOW_METRIC_FULLY_VISIBLE_TIME = 21,
};
/** @} */   /* end of addtogroup screen_windows */

/**
*   @addtogroup screen_debugging
*   @{
*/
/** @brief Types of packets
*
*   Screen packet types are for debugging purposes only. It identifies
*   binary chunks that are used only by the screeninfo utility (a command-
*   line tool in @c /dev/screen/ that is only visible if you have root access)
*   that is used to decode these packets.
*   @anonenum Screen_Packet_Types Screen packet types
*/
enum {
	/** A binary chunk from the request ring buffer (@c /dev/screen/request) */
	SCREEN_REQUEST_PACKET = 0,

	/** A binary chunk from the blit ring buffer or log (@c /dev/screen/0/blt-#/) */
	SCREEN_BLIT_PACKET = 1,

	/** A binary chunk from the input ring buffer or log (@c /dev/screen/input) */
	SCREEN_INPUT_PACKET = 2,

	/** A binary chunk from the event queue (@c /dev/screen/pid/ctx-#) */
	SCREEN_EVENT_PACKET = 3,

	/** A binary chunk from the request ring buffers (errors only) */
	SCREEN_ERROR_PACKET = 4,
};
/** @} */   /* end of ingroup screen_debugging */

/** @brief The types of cursor shapes
*
*   @anonenum Screen_Cursor_Shapes Screen cursor shapes
*/
enum {
	SCREEN_CURSOR_SHAPE_CUSTOM   = -1,	/**< For future use. */
	SCREEN_CURSOR_SHAPE_ARROW    = 0,	/**< An arrow cursor shape. */
	SCREEN_CURSOR_SHAPE_KEYBOARD = 1,	/**< A cursor shape that indicates keyboard. */
	SCREEN_CURSOR_SHAPE_HOME     = 2,	/**< A cursor shape that indicates home. */
	SCREEN_CURSOR_SHAPE_MENU     = 3,	/**< A cursor shape that indicates menu. */
	SCREEN_CURSOR_SHAPE_NONE     = 4,	/**< No cursor shape. */
	SCREEN_CURSOR_SHAPE_HAND     = 5,	/**< A cursor shape that shows a hand. */
	SCREEN_CURSOR_SHAPE_IBEAM    = 6,	/**< A cursor shape that indicates an insertion point. */
	SCREEN_CURSOR_SHAPE_WAIT     = 7,	/**< A cursor shape that indicates waiting action. */
	SCREEN_CURSOR_SHAPE_ZOOM_IN  = 8,	/**< A cursor shape that indicates zooming in action. */
	SCREEN_CURSOR_SHAPE_ZOOM_OUT = 9,	/**< A cursor shape that indicates zooming out action. */
	SCREEN_CURSOR_SHAPE_GRAB     = 10,	/**< A cursor shape that indicates a grab. */
	SCREEN_CURSOR_SHAPE_GRABBING = 11,	/**< A cursor shape that indicates grabbing action. */
	SCREEN_CURSOR_SHAPE_CROSS    = 12,	/**< A cursor shape that shows a cross. */
	SCREEN_CURSOR_SHAPE_MOVE     = 13,	/**< A cursor shape that indicates movement. */
};

/** @brief The types of status for Screen API objects
*
*          Not all status types are applicable to all Screen API objects.
*
*   @anonenum Screen_Status_Types Screen status types
*/
enum {
	/** The Screen API object isn't supported.
	*   Applicable to the following Screen API object(s):
	*     - window (e.g., possibly there's no framebuffer available)
	*/
	SCREEN_STATUS_ENOTSUP       = -48,

	/** The Screen API object isn't accessible due to permissions.
	*   Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_STATUS_EACCES        = -13,

	/** The Screen API object is unavailable.
	*   Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_STATUS_EAGAIN        = -11,

	/** The Screen API object is destroyed.
	*   Applicable to the following Screen API object(s):
	*     - context
	*     - device
	*     - group
	*     - pixmap
	*     - session
	*     - stream
	*     - window
	*/
	SCREEN_STATUS_DESTROYED     = 0,

	/** The Screen API object is created. In some cases, the Screen API object is created, but has no buffers.
	*   Applicable to the following Screen API object(s):
	*     - context
	*     - device
	*     - group
	*     - pixmap
	*     - session
	*     - stream
	*     - window
	*/
	SCREEN_STATUS_CREATED       = 1,

	/** The Screen API object is created. In some cases, the Screen API object is created,
	 *  has at least one buffer.
	*   Applicable to the following Screen API object(s):
	*     - pixmap
	*     - stream
	*     - window
	*/
	SCREEN_STATUS_REALIZED      = 2,

	/** The Screen API object isn't visible.
	*   Applicable to the following Screen API object(s):
	*     - stream
	*     - window
	*/
	SCREEN_STATUS_INVISIBLE     = 3,

	/** The Screen API object is clipped by the framebuffer; therefore, it's not shown on the display.
	*   Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_STATUS_CLIPPED       = 4,

	/** The Screen API object is covered; therefore, it may not be shown on the display.
	*   Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_STATUS_COVERED       = 5,

	/** The Screen API object is visible.
	*   Applicable to the following Screen API object(s):
	*     - window (visible, but isn't necessarily shown on the display)
	*/
	SCREEN_STATUS_VISIBLE       = 9,

	/** The Screen API object is visible, and is shown on the display.
	*   Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_STATUS_FULLY_VISIBLE = 10,

	/** The Screen API object is the same size as the display.
	*   Applicable to the following Screen API object(s):
	*     - window
	*/
	SCREEN_STATUS_FULLSCREEN    = 11,
};

/** @brief The available focus change logic options.

   @anonenum Screen Focus Modes
**/
enum {
	/** Screen can change the which window has focus on a display, such as from a pointer or touch event.
	 *  The display focus can additionally be changed by setting the SCREEN_PROPERTY_FOCUS on a display object.
	 *  Applicable to the following Screen API object(s):
	 *  - display
	 */
	SCREEN_FOCUS_MODE_AUTOMATIC = 0,

	/** Screen can't change which window has the focus on a display.
	 *  The display focus can be controlled by setting the SCREEN_PROPERTY_FOCUS on a display object.
	 *   Applicable to the following Screen API object(s):
	 *     - display
	 */
	SCREEN_FOCUS_MODE_MANUAL = 1
};

/**
*   @ingroup screen_blits
*   @{
*/
/**
*   @brief Copy pixel data from one buffer to another
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function requests pixels from one buffer be copied to another.
*   Note that this blit operation may not execute immediately;
*   therefore, you could still encounter an error when you actually trigger
*   the blit operation even though calls to this function is successful.
*
*   The @c attribs argument is allowed to be NULL or empty (i.e. contains a
*   single element that is set to @c #SCREEN_BLIT_END). When @c attribs is empty
*   or NULL, the attribute defaults are used. See the attribute default values found in
*   <a href="group__screen__blits_1Screen_Blit_Types.xml"> Screen blit types</a>.
*
*   You can change the default behaviour.
*   To do so, set @c attribs with pairings of the
*   following valid tokens and their desired values:
*   - @c #SCREEN_BLIT_SOURCE_X
*   - @c #SCREEN_BLIT_SOURCE_Y
*   - @c #SCREEN_BLIT_SOURCE_WIDTH
*   - @c #SCREEN_BLIT_SOURCE_HEIGHT
*   - @c #SCREEN_BLIT_DESTINATION_X
*   - @c #SCREEN_BLIT_DESTINATION_Y
*   - @c #SCREEN_BLIT_DESTINATION_WIDTH
*   - @c #SCREEN_BLIT_DESTINATION_HEIGHT
*   - @c #SCREEN_BLIT_SCALE_QUALITY
*   - @c #SCREEN_BLIT_GLOBAL_ALPHA
*   - @c #SCREEN_BLIT_ROTATION
*   - @c #SCREEN_BLIT_TRANSPARENCY
*
*   @param  ctx A connection to Screen
*   @param  dst The buffer which data is copied to.
*               \n The source rectangle must be located entirely within the source buffer (src) and the
*               destination rectangle must be located entirely within the destination buffer (dst);
*               otherwise @c screen_blit() returns an error.
*   @param  src The buffer which the pixels is copied from.
*   @param  attribs A list that contains the attributes that define the
*                   blit. This list must consist of a series of token-value pairs
*                   terminated with a @c #SCREEN_BLIT_END token. The tokens used
*                   in this list must be of type
*                   <a href="group__screen__blits_1Screen_Blit_Types.xml">Screen blit types</a>.
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_blit(screen_context_t ctx, screen_buffer_t dst, screen_buffer_t src, const int *attribs);

/**
*   @brief Fill an area of a specified buffer
*
*   <b>Function Type:</b> <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function requests that a rectangular area of the destination buffer be
*   filled with a solid color. Note that this fill operation may not execute immediately;
*   therefore, you could still encounter an error when you actually trigger
*   the fill operation even though calls to this function is successful.
*
*   The @c attribs argument is allowed to be NULL or empty (i.e. contains a
*   single element that is set to @c #SCREEN_BLIT_END). When @c attribs is empty
*   or NULL, the attribute defaults are used. See the attribute default values found in
*   <a href="group__screen__blits_1Screen_Blit_Types.xml"> Screen blit types</a>.
*
*   You can change the default behaviour.
*   To do so, set @c attribs with pairings of the
*   following valid tokens and their desired values:
*   - @c #SCREEN_BLIT_DESTINATION_X
*   - @c #SCREEN_BLIT_DESTINATION_Y
*   - @c #SCREEN_BLIT_DESTINATION_WIDTH
*   - @c #SCREEN_BLIT_DESTINATION_HEIGHT
*   - @c #SCREEN_BLIT_GLOBAL_ALPHA
*   - @c #SCREEN_BLIT_COLOR
*
*   @param  ctx A connection to Screen
*   @param  dst The buffer which data is copied to.
*   @param  attribs A list that contains the attributes that define the
*                   blit. This list must consist of a series of token-value pairs
*                   terminated with a @c #SCREEN_BLIT_END token. The tokens used
*                   in this list must be of type
*                   <a href="group__screen__blits_1Screen_Blit_Types.xml">Screen blit types</a>.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_fill(screen_context_t ctx, screen_buffer_t dst, const int *attribs);

/**
*   @brief Flush all the blits issued
*
*   <b>Function Type:</b> <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function flushes all delayed blits and fills since the last call to this
*   function, or since the last call to @c screen_post_window(). Note that this is a
*   flush of delayed blits and does not imply a flush of the command buffer.
*   The blits will start executing shortly after you call the function. The blits
*   may not be complete when the function returns, unless the @c #SCREEN_WAIT_IDLE
*   flag is set. This function has no effect on other non-blit delayed calls. The
*   @c screen_post_window() function performs an implicit flush of any pending blits.
*   The content that is to be presented via the call to @c screen_post_window()
*   is most likely the result of any pending blit operations completing.
*
*   The connection to Screen must have been acquired with the function
*   @c screen_create_context().
*
*   @param  ctx A connection to Screen
*   @param  flags The indication of how you want this function to behave. Valid values for this
*                 parameter are:
*                 - @c #SCREEN_WAIT_IDLE: When you use @c #SCREEN_WAIT_IDLE, this function blocks,
*                                         and returns only after Screen has completed all blits.
*                 - 0: When you use 0, this function returns immediately; blits may not be
*                      completed.

*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_flush_blits(screen_context_t ctx, int flags);
/** @} */   /* end of ingroup screen_blits */

/**
*   @ingroup screen_buffers
*   @{
*/

/**
*   @brief Wait for the next buffer to become available from a producer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function is typically used by consumers to get the next frame posted
*   by the producer. The consumer uses a stream that shares buffers
*   with an external stream handle by calling @c screen_consume_stream_buffers().
*   Producers and consumers that reside in the same process can use the API, or simply pass the
*   buffers between each other.
*
*   Acquiring a buffer doesn't release previously acquired buffers unless you specify
*   @c #SCREEN_ACQUIRE_AND_RELEASE_OTHERS in the @c flags parameter. In this case, all other
*   buffers are released whenever a new buffer is returned. Typically,
*   after calling @c screen_acquire_buffer(), the consumer releases the previously acquired
*   buffer by calling @c screen_release_buffer(). Note that if the stream is single-buffered, the
*   consumer calls @c screen_release_buffer() after finishing with the buffer.
*
*   This function returns the next available buffer. However, if the stream is in
*   @c #SCREEN_STREAM_MODE_MAILBOX mode, this function returns all queued front buffers to the
*   render queue, except for the last buffer. If no buffer is available, this function blocks unless
*   you specify @c #SCREEN_ACQUIRE_DONT_BLOCK in the @c flags parameter.
*
*   If you provide a non-NULL pointer in @c pcount and @c pdirty_rects, this function
*   returns the number of regions in the location pointed to by @c pcount and @c pdirty_rects
*   will point to an array of integers that represents the dirty regions. This array is allocated
*   dynamically and must be freed when it's no longer needed.
*
*   If you provide NULL in @c pcount and @c pdirty_rects, this function doesn't make the dirty
*   regions (if any) available to the consumer.
*
*   @param  pbuf A pointer to the buffer that was acquired; or NULL, when no buffer was
*                available and you specified @c #SCREEN_ACQUIRE_DONT_BLOCK in @c flags.
*   @param  stream The handle of a stream from which a buffer needs to be
*                  acquired. The stream must have been created with @c screen_create_stream().
*                  This stream is typically realized with @c screen_consume_stream_buffers().
*   @param  pcount A pointer to the number of dirty regions posted by the producer.
*                  If @c pcount points to 0 (i.e., @c *pcount == 0) when it returns from this function,
*                  the consumer should consider the entire @c pbuf to be dirty.
*   @param  pdirty_rects A pointer to an array of @c pcount regions corresponding to the dirty
*                        regions posted by the producer. Each region is expressed by a set of 4
*                        integers that represent the x1, y1, x2, and y2 values of a rectangle.
*                        If @c *pdirty_rects is NULL (i.e., @c *pdirty_rects == NULL) when it returns
*                        from this function, the consumer should consider the entire @c pbuf to be dirty.
*   @param  pflags A pointer to an integer that represents the bitmask that Screen used when the producer
*                  posted (e.g., producer called @c screen_post_window() or @c screen_post_stream()). The integer
*                  is a bitmask that represents a combination of
*                  <a href="screen_8h_1Screen_Flushing_Types.xml">Screen flushing types</a>.
*                  As a consumer, use @c pflags to determine if any action needs to be taken
*                  to prevent the producer from remaining blocked.
*                  Pass NULL if you don't intend to use this information.
*   @param  flags The indication of how you want this function to behave. This integer is a bitmask
*                 that represents the combination of
*                 <a href="screen_8h_1Screen_Acquire_Flags.xml">Screen acquire buffer flags</a>:
*                 - @c #SCREEN_ACQUIRE_DONT_BLOCK: When you use @c #SCREEN_ACQUIRE_DONT_BLOCK, this
*                                                  function returns immediately with the available
*                                                  buffer in @c pbuf. If there are no buffers
*                                                  available, this function returns immediately with
*                                                  <tt>*pbuf = NULL;</tt>.
*                 - @c #SCREEN_ACQUIRE_AND_RELEASE_OTHERS: When you use this option, this function
*                                                          first releases all buffers that you've previously
*                                                          acquired, before acquring a new one.
*                                                          This flag must be specified with
*                                                          @c #SCREEN_ACQUIRE_DONT_BLOCK.
*                 - 0: When you use 0, this function blocks for as long as there are no buffers
*                      that are available for acquiring.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_acquire_buffer(screen_buffer_t *pbuf, screen_stream_t stream, int *pcount, int **pdirty_rects, int *pflags, int flags);

/**
*   @brief Create a buffer handle that can later be attached to a window or a pixmap
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a buffer object, which describes memory where pixels
*   can be drawn to or read from. Applications must use @c screen_destroy_buffer()
*   when a buffer is no longer used.
*
*   @param  pbuf An address where the function can store a handle for the native
*                buffer.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_buffer(screen_buffer_t *pbuf);

/**
*   @brief Destroy a buffer and frees associated resources
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function destroys the buffer object associated with the buffer handle.
*   Any resources created for this buffer is also released. The buffer
*   handle can no longer be used as argument in subsequent Screen API calls. The
*   actual memory buffer described by this buffer handle isn't released by this
*   operation. The application is responsible for freeing its own external
*   buffers. Only buffers created with @c screen_create_buffer() must be
*   destroyed with this function.
*
*   @param  buf The handle of the buffer you want to destroy. This buffer must
*               have been created with @c screen_create_buffer().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_buffer(screen_buffer_t buf);

/**
*   @brief Retrieve the current value of the specified buffer property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function stores the current value of a buffer property in a user-provided buffer.
*
*   Currently there are no buffer properties that can be retrieved using this
*   function.
*
*   @param  buf The handle of the buffer whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The maximum number of bytes that Screen can write to @c param.
*   @param  param The buffer where Screen stores the retrieved value (a character array that
*                 includes a terminating null character). This buffer must be an array of type
*                 @c char with a minimum length of @c len. If this user-provided buffer has
*                 insufficient space for storing the property being retrieved, Screen
*                 truncates the retrieved value and returns the character array without a
*                 terminating null character.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_buffer_property_cv(screen_buffer_t buf, int pname, int len, char *param);

/**
*   @brief Retrieve the current value of the specified buffer property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function stores the current value of a buffer property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_BUFFER_SIZE
*   - @c #SCREEN_PROPERTY_FORMAT
*   - @c #SCREEN_PROPERTY_ID
*   - @c #SCREEN_PROPERTY_INTERLACED
*   - @c #SCREEN_PROPERTY_PHYSICALLY_CONTIGUOUS
*   - @c #SCREEN_PROPERTY_PLANAR_OFFSETS
*   - @c #SCREEN_PROPERTY_PROTECTED
*   - @c #SCREEN_PROPERTY_SIZE
*   - @c #SCREEN_PROPERTY_STRIDE
*   - @c #SCREEN_PROPERTY_USAGE
*
*   @param  buf The handle of the buffer whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c int. The buffer may be a single integer or
*                 an array of integers, depending on the property being retrieved.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_buffer_property_iv(screen_buffer_t buf, int pname, int *param);

/**
*   @brief Retrieve the current value of the specified buffer property of type
*          @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function stores the current value of a buffer property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_PHYSICAL_ADDRESS
*
*   @param  buf The handle of the buffer whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_buffer_property_llv(screen_buffer_t buf, int pname, long long *param);

/**
*   @brief Retrieve the current value of the specified buffer property of type @c void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function stores the current value of a buffer property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_EGL_HANDLE
*   - @c #SCREEN_PROPERTY_NATIVE_IMAGE
*   - @c #SCREEN_PROPERTY_POINTER
*
*   @param  buf The handle of the buffer whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_buffer_property_pv(screen_buffer_t buf, int pname, void **param);

/**
*   @brief Allow a buffer to become a render buffer again
*
*   <b>Function Type:</b> <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function should be used when @c screen_post_stream() or @c screen_post_window()
*   is called with the @c #SCREEN_EXPLICIT_ENQUEUE flag.  It may also be called when a
*   buffer obtained with @c screen_dequeue_window_render_buffer() or
*   @c screen_dequeue_stream_render_buffer() will not be posted.  This action is
*   performed automatically by @c screen_post_stream() or @c screen_post_window()
*   when those functions are called without the @c #SCREEN_EXPLICIT_ENQUEUE flag.
*   A call to this function indicates that the producer is done with the buffer.
*
*   @param  buf The handle of the buffer that may become a render buffer again.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_enqueue_render_buffer(screen_buffer_t buf);

/**
*   @brief Return a buffer to the producer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function is typically used by consumers to indicate that they are no longer using a buffer.
*   This function returns the buffer to the producer and it becomes available the next time the
*   producer posts a frame. If the producer is blocked waiting for a render buffer, it will unblock
*   after the last of the consumers, that have acquired this buffer, have released the buffer.
*
*   Note that acquiring a buffer doesn't implicitly release the previously
*   acquired buffer. Each consumer is responsible for calling @c screen_release_buffer()
*   each time it's finished with a buffer. The consumer can call
*   @c screen_release_buffer() immediately after using the buffer one time, or it
*   can release the buffer only when it has acquired a new buffer to replace
*   the old one. Consumers of single-buffered streams must call
*   @c screen_release_buffer() immediately after using the latest frame.
*
*   This function returns an error when it's called with a buffer that wasn't previously
*   acquired, or with a buffer that has already been released.
*
*   @param  buf The buffer that needs to be released.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_release_buffer(screen_buffer_t buf);

/**
*   @brief Set the value of the specified buffer property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function sets the value of a buffer property from a user-provided buffer.
*
*   Currently there are no buffer properties that can be set using this function.
*
*   @param  buf The handle of the buffer whose property is to be set.
*               The buffer must have been created with the function
*               @c screen_create_buffer().
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The size (including a terminating null character) of @c param.
*               Screen reads from @c param until it encounters a null character (`\0`), or until
*               @c len number of bytes is reached. Ensure that @c len doesn't exceed the maximum
*               size that Screen allows for the property specified by @c pname.
*   @param  param A pointer to a character array containing the new property value. This array
*                 must have a minimum length of @c len.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_buffer_property_cv(screen_buffer_t buf, int pname, int len, const char *param);

/**
*   @brief Set the value of the specified buffer property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function sets the value of a buffer property from a user-provided
*   buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_BUFFER_SIZE
*   - @c #SCREEN_PROPERTY_FD
*   - @c #SCREEN_PROPERTY_FORMAT
*   - @c #SCREEN_PROPERTY_INTERLACED
*   - @c #SCREEN_PROPERTY_PHYSICALLY_CONTIGUOUS
*   - @c #SCREEN_PROPERTY_PLANAR_OFFSETS
*   - @c #SCREEN_PROPERTY_PROTECTED
*   - @c #SCREEN_PROPERTY_SIZE
*   - @c #SCREEN_PROPERTY_STRIDE
*
*   @param  buf The handle of the buffer whose property is to be set. The buffer
*               must have been created with the function @c screen_create_buffer().
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c int. @c param may be a single integer or
*                 an array of integers depending on the property being set.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_buffer_property_iv(screen_buffer_t buf, int pname, const int *param);

/**
*   @brief Set the value of the specified buffer property of type @c long @c long
*          integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function sets the value of a buffer property from a user-provided
*   buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_PHYSICAL_ADDRESS
*
*   @param  buf The handle of the buffer whose property is to be set.
*               The buffer must have been created with the function
*               @c screen_create_buffer().
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_buffer_property_llv(screen_buffer_t buf, int pname, const long long *param);

/**
*   @brief Set the value of the specified buffer property of type void*
*
*   <b>Function Type:</b> <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function sets the value of a buffer property from a user-provided
*   buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_EGL_HANDLE
*   - @c #SCREEN_PROPERTY_NATIVE_IMAGE
*   - @c #SCREEN_PROPERTY_POINTER
*
*   @param  buf The handle of the buffer whose property is to be set.
*               The buffer must have been created with the function
*               @c screen_create_buffer().
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_buffer_property_pv(screen_buffer_t buf, int pname, void **param);
/** @} */   /* end of ingroup screen_buffers */

/**
*   @ingroup screen_contexts
*   @{
**/

/**
*   @brief Establish a connection with Screen
*
*   <b>Function type:</b> <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function establishes communication with Screen.  This function opens @c /dev/screen and
*   sends the proper connect sequence. If the call succeeds, memory is allocated to store the
*   context state.  Screen creates an event queue and associates it with the connecting process.
*
*   @param  pctx A pointer to a @c screen_context_t where a handle for the new
*           context can be stored.
*   @param  flags The type of context to be created. This integer is a bitmask that represents the
*                 combination of
*                 <a href="group__screen__contexts_1Screen_Context_Types.xml">Screen context types</a>.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_context(screen_context_t *pctx, int flags);

/**
*   @brief Terminate a connection with Screen
*
*   <b>Function type:</b> <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function closes an existing connection with Screen; therefore this context can no longer
*   be used. The context is freed, along with any locally allocated resources. All render targets
*   associated with this connection are destroyed. All events that are waiting in the event queue
*   and any pending asynchronous commands are discarded. This operation doesn't flush the command
*   buffer.
*
*   @param  ctx The connection to Screen that's to be terminated. This context
*               must have been created with @c screen_create_context().
*
*   @return 0
*/
int screen_destroy_context(screen_context_t ctx);

/**
*   @brief Flush a context, given a context and a set of flags
*
*   <b>Function type:</b> <a href="manual/rscreen_apply_execution.xml">Apply Execution</a>
*
*   This function flushes any delayed command and causes the contents of
*   displays to be updated, when applicable.
*
*   If debugging, you can call this function after all delayed function calls
*   as a way to determine the exact function call which may have caused an error.
*
*   @param  ctx The connection to Screen that is to be flushed. This context
*               must have been created with @c screen_create_context().
*   @param  flags The indication of how you want this function to behave. Valid values for this
*                 parameter are:
*                 - @c #SCREEN_WAIT_IDLE: When you use @c #SCREEN_WAIT_IDLE, this function blocks,
*                                         and returns only after Screen has updated the contents of
*                                         all affected displays.
*                 - 0: When you use 0, this function returns immediately; blits may not be
*                      completed.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_flush_context(screen_context_t ctx, int flags);

/**
*   @brief Retrieve the current value of the specified context property of type char
*
*   <b>Function Type:</b> <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a context property in a
*   user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_ID
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_KEYMAP
*
*   @param  ctx The handle of the context whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The maximum number of bytes that Screen can write to @c param.
*   @param  param The buffer where Screen stores the retrieved value (a character array that
*                 includes a terminating null character). This buffer must be an array of type
*                 @c char with a minimum length of @c len. If this user-provided buffer has
*                 insufficient space for storing the property being retrieved, Screen
*                 truncates the retrieved value and returns the character array without a
*                 terminating null character.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_context_property_cv(screen_context_t ctx, int pname, int len, char *param);

/**
*   @brief Retrieve the current value of the specified context property of type
*          integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a context property in a
*   user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_DEVICE_COUNT [1]
*   - @c #SCREEN_PROPERTY_DISPLAY_COUNT [1]
*   - @c #SCREEN_PROPERTY_GROUP_COUNT [1]
*   - @c #SCREEN_PROPERTY_ID [1]
*   - @c #SCREEN_PROPERTY_IDLE_STATE
*   - @c #SCREEN_PROPERTY_OWNER_PID [1]
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_PIXMAP_COUNT [1]
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_SESSION_COUNT [1]
*   - @c #SCREEN_PROPERTY_STATUS
*   - @c #SCREEN_PROPERTY_STREAM_COUNT [1]
*   - @c #SCREEN_PROPERTY_TYPE
*   - @c #SCREEN_PROPERTY_WINDOW_COUNT [1]
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   @param  ctx The handle of the context whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c int. The buffer may be a single integer or
*                 an array of integers, depending on the property being retrieved.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_context_property_iv(screen_context_t ctx, int pname, int *param);

/**
*   @brief Retrieve the current value of the specified context property of type
*          @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a context property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_IDLE_TIMEOUT
*
*   @param  ctx The handle of the context whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_context_property_llv(screen_context_t ctx, int pname, long long *param);

/**
*   @brief Retrieve the current value of the specified context property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a context property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_DEVICES [1]
*   - @c #SCREEN_PROPERTY_DISPLAYS [1]
*   - @c #SCREEN_PROPERTY_FOCUS
*   - @c #SCREEN_PROPERTY_GROUPS [1]
*   - @c #SCREEN_PROPERTY_MTOUCH_FOCUS
*   - @c #SCREEN_PROPERTY_PIXMAPS [1]
*   - @c #SCREEN_PROPERTY_POINTER_FOCUS
*   - @c #SCREEN_PROPERTY_SESSIONS [1]
*   - @c #SCREEN_PROPERTY_STREAMS[1]
*   - @c #SCREEN_PROPERTY_WINDOWS[1]
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   @param  ctx The handle of the context whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_context_property_pv(screen_context_t ctx, int pname, void **param);

/**
*   @brief Request Screen to send asynchronous notifications
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function requests Screen to send asynchronous notifications to your
*   application; it behaves similarly to
*   <a href="../../com.qnx.doc.neutrino.lib_ref/topic/i/ionotify.html">ionotify()</a>.
*
*   You must call screen_register_event() to register your event before you can call screen_notify().
*
*   Specify which type of notification your application wants to receive using the @c flags argument.
*   Valid notification types are of type
*   <a href="group__screen__contexts_1Screen_Notification_Types.xml">Screen notification types</a>.
*
*   Calling this function disables any previous calls to @c screen_notify()
*   that were called with the same combination of arguments: @c ctx, @c flags,
*   and @c obj.
*
*   If necessary, notifications can be filtered by your application. For example,
*   events specific to a keyboard, can be detected by passing the appropriate
*   @c screen_device_t handle in the @c obj argument. Refer to the
*   <em>Screen Developer's Guide</em> for more information.
*
*   @param  ctx The handle of the context that @c obj belongs to.
*   @param  flags The type of notification that you want to be notified of.
*                 Valid notification types are of type
*                 <a href="group__screen__contexts_1Screen_Notification_Types.xml">Screen notification types</a>.
*   @param  obj The object that the notification is for. Not all object types are valid for each notification type.
*               Refer to <a href="group__screen__contexts_1Screen_Notification_Types.xml">Screen notification types</a>
*               to determine which object type is valid for the notification you're registering for.
*   @param  event The notification event of type
*                 <a href="../../com.qnx.doc.neutrino.lib_ref/topic/s/sigevent.html">sigevent</a>.
*                 Refer to @c sys/siginfo.h for more details. Set this argument to
*                 NULL or its type to @c SIGEV_NONE (e.g., use @c SIGEV_NONE_INIT()) to disable
*                 future notifications for specified @c flags from Screen.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_notify(screen_context_t ctx, int flags, const void *obj, const struct sigevent *event);

/**
*   @brief Set the value of the specified context property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a context property from a user-provided buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_KEYMAP
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*
*   @param  ctx The handle of the context whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The size (including a terminating null character) of @c param.
*               Screen reads from @c param until it encounters a null character (`\0`), or until
*               @c len number of bytes is reached. Ensure that @c len doesn't exceed the maximum
*               size that Screen allows for the property specified by @c pname.
*   @param  param A pointer to a character array containing the new property value. This array
*                 must have a minimum length of @c len.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_context_property_cv(screen_context_t ctx, int pname, int len, const char *param);

/**
*   @brief Set the value of the specified context property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a context property from a user-provided buffer.
*
*   You can use this function to set the @c #SCREEN_PROPERTY_PERMISSIONS property.
*
*   @param  ctx The handle of the context whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of @c type int. @c param may be a single integer or
*                 an array of integers depending on the property being set.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_context_property_iv(screen_context_t ctx, int pname, const int *param);

/**
*   @brief Set the value of the specified context property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a context property from a user-provided buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_IDLE_TIMEOUT
*
*   @param  ctx The handle of the context whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_context_property_llv(screen_context_t ctx, int pname, const long long *param);

/**
*   @brief Set the value of the specified context property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a context property from a user-provided buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_FOCUS
*   - @c #SCREEN_PROPERTY_MTOUCH_FOCUS
*   - @c #SCREEN_PROPERTY_POINTER_FOCUS
*
*   @param  ctx The handle of the context whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_context_property_pv(screen_context_t ctx, int pname, void **param);

/**
*   @brief Register a context as the window manager of a display
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function registers the context as the window manager of a display. The context must have
*   @c #SCREEN_WINDOW_MANAGER_CONTEXT privileges. The display must not currently have a window
*   manager.
*
*   This function is associated with the preprocessor definition @c screen_register_window_manager.
*
*   @param  ctx The context which is registering as the window manager.
*   @param  disp The display which the context wants to manage.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_register_window_manager(screen_context_t ctx, screen_display_t disp);

/**
*   @brief Unregister a context as the window manager of a display
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function unregisters the context as the window manager of a display. The context
*   must currently be the window manager for the display.
*
*   This function is associated with the preprocessor definition @c screen_register_window_manager.
*
*   @param  ctx The context which is unregistering as the window manager.
*   @param  disp The display which the context wants to stop managing.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_unregister_window_manager(screen_context_t ctx, screen_display_t disp);

/**
*   @brief Register a @c sigevent for use with asynchronous notifications
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function registers a @c sigevent with Screen so that @c screen_notify() can be used with the event;
*   it's associated with the preprocessor definition @c screen_register_event.
*
*   @param  ctx The context which is registering the @c sigevent and will later be used for @c screen_notify().
*   @param  event The @c sigevent which needs to be registered.
*
*   @return @c 0 if successful, or @c -1 if an error occured
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_register_event(screen_context_t ctx, struct sigevent *event);

/**
*   @brief Unregister a @c sigevent from use with asynchronous notifications
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function unregisters a @c sigevent from Screen;
*   it's associated with the preprocessor definition @c screen_register_event.
*
*   We recommend that you call @c screen_unregister_event() only after you've disabled asynchronous
*   notifications that are using the event you are unregistering. If you remove the event registration while
*   you still have asynchronous notifications using that event enabled, you may not receive any notifications.
*   To disable asynchronous notifications, call @c screen_notify() with the @c event argument set to NULL, or
*   its type set to @c SIGEV_NONE (e.g., use @c SIGEV_NONE_INIT()) for those notifications that were enabled.
*
*   @param  event The @c sigevent that you're unregistering.
*
*   @return @c 0 if successful, or @c -1 if an error occured
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_unregister_event(struct sigevent *event);

/** @} */   /* end of ingroup screen_contexts */

/**
*   @ingroup screen_devices
*   @{
*/

/**
*   @brief Create a device of specified type to be associated with a context
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a input device object to be associated with a context. Note that you
*   need to be within a privileged context to call this function. Applications must use
*   @c screen_destroy_device() when a device is no longer used.
*   The following are valid input devices that can be created:
*   - @c #SCREEN_EVENT_GAMEPAD
*   - @c #SCREEN_EVENT_JOG
*   - @c #SCREEN_EVENT_JOYSTICK
*   - @c #SCREEN_EVENT_KEYBOARD
*   - @c #SCREEN_EVENT_MTOUCH_TOUCH
*   - @c #SCREEN_EVENT_POINTER
*
*   @param  pdev A pointer to a @c #screen_device_t where a handle for the new
*                input device can be stored.
*   @param  ctx The handle of the context in which the input device is to be
*               created. This context must have been created with the context type
*               of @c #SCREEN_INPUT_PROVIDER_CONTEXT using @c screen_create_context().
*   @param  type The type of input device to be created.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_device_type(screen_device_t *pdev, screen_context_t ctx, int type);

/**
*   @brief Destroy an input device and free associated resources
*
*   <b>Function Type:</b>
*   - <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a> or
*   - <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   If @c dev was created with @c screen_create_device_type(), then
*   the function type for @c screen_destroy_device() is flushing execution.
*   Otherwise, this function type is immediate execution.
*
*   This function destroys the input device object associated with the device
*   handle, @c dev. Any resources created for this input device are released.
*
*   All input devices, whether created with @c screen_create_device_type() or
*   not, must be destroyed with this function. If your input device wasn't
*   created with @c screen_create_device_type(), it must be destroyed upon the
*   receipt of a @c #SCREEN_EVENT_CLOSE event. When the function type for
*   @c screen_destroy_device() is flushing execution, any entries in the command
*   buffer that have reference to this device are flushed and processed before
*   destroying the device.
*
*   @param  dev The handle of the input device that you want to destroy.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_device(screen_device_t dev);

/**
*   @brief Retrieve the current value of the specified device property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a device property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_ID
*   - @c #SCREEN_PROPERTY_KEYMAP
*   - @c #SCREEN_PROPERTY_PRODUCT
*   - @c #SCREEN_PROPERTY_VENDOR
*
*   @param  dev The handle of the device whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The maximum number of bytes that Screen can write to @c param.
*   @param  param The buffer where Screen stores the retrieved value (a character array that
*                 includes a terminating null character). This buffer must be an array of type
*                 @c char with a minimum length of @c len. If this user-provided buffer has
*                 insufficient space for storing the property being retrieved, Screen
*                 truncates the retrieved value and returns the character array without a
*                 terminating null character.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_device_property_cv(screen_device_t dev, int pname, int len, char *param);

/**
*   @brief Retrieve the current value of the specified device property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a device property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_ACCELERATION (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_ANALOG0 (for @c #SCREEN_EVENT_GAMEPAD and @c #SCREEN_EVENT_JOYSTICK only)
*   - @c #SCREEN_PROPERTY_ANALOG1 (for @c #SCREEN_EVENT_GAMEPAD and @c #SCREEN_EVENT_JOYSTICK only)
*   - @c #SCREEN_PROPERTY_ANALOG_COUNT (for @c #SCREEN_EVENT_GAMEPAD and @c #SCREEN_EVENT_JOYSTICK only)
*   - @c #SCREEN_PROPERTY_BALLISTIC_COEFFICIENTS (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_BUTTONS (for all events except @c #SCREEN_EVENT_KEYBOARD)
*   - @c #SCREEN_PROPERTY_BUTTON_COUNT
*   - @c #SCREEN_PROPERTY_BUTTON_MAP (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_DPI (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_FLOATING (for @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_GESTURE_PARAMETERS (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_ID [1]
*   - @c #SCREEN_PROPERTY_IDLE_STATE
*   - @c #SCREEN_PROPERTY_KEYMAP_PAGE
*   - @c #SCREEN_PROPERTY_MAXIMUM_TOUCH_ID (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_METRIC_COUNT [1]
*   - @c #SCREEN_PROPERTY_MODE (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_MODIFIERS
*   - @c #SCREEN_PROPERTY_OWNER_PID [1]
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_PHYSICAL_SIZE (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_POWER_MODE
*   - @c #SCREEN_PROPERTY_PRODUCT
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR [1]
*   - @c #SCREEN_PROPERTY_SIZE (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, @c #SCREEN_EVENT_MTOUCH_TOUCH, and @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_SPEED (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_STATUS
*   - @c #SCREEN_PROPERTY_STICKY_MODIFIERS
*   - @c #SCREEN_PROPERTY_TRANSFORM (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_TYPE
*   - @c #SCREEN_PROPERTY_VENDOR
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   @param  dev The handle of the device whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c int. The buffer may be a single integer or
*                 an array of integers, depending on the property being retrieved.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_device_property_iv(screen_device_t dev, int pname, int *param);

/**
*   @brief Retrieve the current value of the specified device property of type
*          @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a device property in a user-provided buffer.
*   The values of the following properties can be queried using this function:
*    - @c #SCREEN_PROPERTY_IDLE_TIMEOUT
*    - @c #SCREEN_PROPERTY_METRICS
*    - @c #SCREEN_PROPERTY_REPEAT_DELAY
*    - @c #SCREEN_PROPERTY_REPEAT_RATE
*
*   @param  dev The handle of the device whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_device_property_llv(screen_device_t dev, int pname, long long *param);

/**
*   @brief Retrieve the current value of the specified device property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a device property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_CONTEXT [1]
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_SESSION
*   - @c #SCREEN_PROPERTY_USER_HANDLE [1]
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   @param  dev The handle of the device whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_device_property_pv(screen_device_t dev, int pname, void **param);

/**
*   @brief Retrieve the current value of the specified device property of type @c float
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a device property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_BALLISTIC_COEFFICIENTS (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_TRANSFORM (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   @param  dev The handle of the device whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c float.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_device_property_fv(screen_device_t dev, int pname, float *param);

/**
*   @brief Set the value of the specified device property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a device property from a user-provided buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_KEYMAP
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_PRODUCT
*   - @c #SCREEN_PROPERTY_VENDOR
*
*   @param  dev The handle of the device whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The size (including a terminating null character) of @c param.
*               Screen reads from @c param until it encounters a null character (`\0`), or until
*               @c len number of bytes is reached. Ensure that @c len doesn't exceed the maximum
*               size that Screen allows for the property specified by @c pname.
*   @param  param A pointer to a character array containing the new property value. This array
*                 must have a minimum length of @c len.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_device_property_cv(screen_device_t dev, int pname, int len, const char *param);

/**
*   @brief Set the value of the specified device property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a device property from a user-provided buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ACCELERATION (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_BALLISTIC_COEFFICIENTS (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_BUTTON_COUNT
*   - @c #SCREEN_PROPERTY_BUTTON_MAP (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_DPI (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_FLOATING (for @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_GESTURE_PARAMETERS (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_KEYMAP_PAGE
*   - @c #SCREEN_PROPERTY_MODE (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_MODIFIERS
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_PHYSICAL_SIZE (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_POWER_MODE
*   - @c #SCREEN_PROPERTY_PRODUCT
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR
*   - @c #SCREEN_PROPERTY_SIZE (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, @c #SCREEN_EVENT_MTOUCH_TOUCH, and @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_SPEED (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_POINTER only)
*   - @c #SCREEN_PROPERTY_STICKY_MODIFIERS
*   - @c #SCREEN_PROPERTY_TRANSFORM (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_VENDOR
*
*   @param  dev The handle of the device whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c int. @c param may be a single integer or
*                 an array of integers depending on the property being set.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_device_property_iv(screen_device_t dev, int pname, const int *param);

/**
*   @brief Set the value of the specified device property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a device property from a user-provided buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_IDLE_TIMEOUT
*   - @c #SCREEN_PROPERTY_REPEAT_DELAY
*   - @c #SCREEN_PROPERTY_REPEAT_RATE
*
*   @param  dev The handle of the device whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_device_property_llv(screen_device_t dev, int pname, const long long *param);

/**
*   @brief Set the value of the specified device property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a device property from a user-provided buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_SESSION
*   - @c #SCREEN_PROPERTY_USER_HANDLE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   @param  dev The handle of the device whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_device_property_pv(screen_device_t dev, int pname, void **param);

/**
*   @brief Set the value of the specified device property of type @c float
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a device property from a user-provided buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_BALLISTIC_COEFFICIENTS (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_TRANSFORM (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*
*   @param  dev The handle of the device whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c float.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_device_property_fv(screen_device_t dev, int pname, const float *param);
/** @} */   /* end of ingroup screen_devices */

/*
 * Displays
 */
/**
*   @ingroup screen_displays
*   @{
*/

/**
*   @brief Retrieve the current value of the specified display property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a display property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_PRODUCT
*   - @c #SCREEN_PROPERTY_VENDOR
*
*   @param  disp The handle of the display whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The maximum number of bytes that Screen can write to @c param.
*   @param  param The buffer where Screen stores the retrieved value (a character array that
*                 includes a terminating null character). This buffer must be an array of type
*                 @c char with a minimum length of @c len. If this user-provided buffer has
*                 insufficient space for storing the property being retrieved, Screen
*                 truncates the retrieved value and returns the character array without a
*                 terminating null character.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_display_property_cv(screen_display_t disp, int pname, int len, char *param);

/**
*   @brief Retrieve the current value of the specified display property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a display property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_ATTACHED
*   - @c #SCREEN_PROPERTY_BLUE_GAMMA_CURVE [2]
*   - @c #SCREEN_PROPERTY_BRIGHTNESS
*   - @c #SCREEN_PROPERTY_CBABC_MODE
*   - @c #SCREEN_PROPERTY_CHROMATICITY_COORDS
*   - @c #SCREEN_PROPERTY_CONTRAST
*   - @c #SCREEN_PROPERTY_DETACHABLE
*   - @c #SCREEN_PROPERTY_DPI
*   - @c #SCREEN_PROPERTY_FOCUS
*   - @c #SCREEN_PROPERTY_FORMATS
*   - @c #SCREEN_PROPERTY_FORMAT_COUNT
*   - @c #SCREEN_PROPERTY_GAMMA [2]
*   - @c #SCREEN_PROPERTY_GREEN_GAMMA_CURVE [2]
*   - @c #SCREEN_PROPERTY_HUE
*   - @c #SCREEN_PROPERTY_ID [1]
*   - @c #SCREEN_PROPERTY_IDLE_STATE
*   - @c #SCREEN_PROPERTY_INTENSITY
*   - @c #SCREEN_PROPERTY_KEEP_AWAKES
*   - @c #SCREEN_PROPERTY_METRIC_COUNT [1]
*   - @c #SCREEN_PROPERTY_MIRROR_MODE
*   - @c #SCREEN_PROPERTY_MODE_COUNT
*   - @c #SCREEN_PROPERTY_NATIVE_RESOLUTION
*   - @c #SCREEN_PROPERTY_OWNER_PID [1]
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_PHYSICAL_SIZE
*   - @c #SCREEN_PROPERTY_POSITION
*   - @c #SCREEN_PROPERTY_POWER_MODE
*   - @c #SCREEN_PROPERTY_PROTECTION_ENABLE
*   - @c #SCREEN_PROPERTY_RED_GAMMA_CURVE [2]
*   - @c #SCREEN_PROPERTY_ROTATION
*   - @c #SCREEN_PROPERTY_SATURATION
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR [1]
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_SIZE
*   - @c #SCREEN_PROPERTY_SOURCE_SIZE
*   - @c #SCREEN_PROPERTY_TECHNOLOGY
*   - @c #SCREEN_PROPERTY_TRANSPARENCY
*   - @c #SCREEN_PROPERTY_TYPE
*   - @c #SCREEN_PROPERTY_VIEWPORT_POSITION
*   - @c #SCREEN_PROPERTY_VIEWPORT_SIZE
*   - @c #SCREEN_PROPERTY_WINDOW_MANAGER_ID
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   Note [2]: You must be working within a privileged context of type
*             @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able get set this property.
*
*   @param  disp The handle of the display whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*           <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c int. The buffer may be a single integer or
*                 an array of integers, depending on the property being retrieved.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_display_property_iv(screen_display_t disp, int pname, int *param);

/**
*   @brief Retrieve the current value of the specified display property of type
*          @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a display property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_IDLE_TIMEOUT
*   - @c #SCREEN_PROPERTY_METRICS
*
*   @param  disp The handle of the device whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_display_property_llv(screen_display_t disp, int pname, long long *param);

/**
*   @brief Retrieve the current value of the specified display property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a display property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_CONTEXT [1]
*   - @c #SCREEN_PROPERTY_FOCUS [2]
*   - @c #SCREEN_PROPERTY_MODE
*   - @c #SCREEN_PROPERTY_USER_HANDLE [1]
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   Note [2]: You must be working within a privileged context of type
*             @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able to retrieve these above properties.
*
*   @param  disp The handle of the display whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_display_property_pv(screen_display_t disp, int pname, void **param);

/**
*   @brief Retrieve the current value of the specified display property of type @c float
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a display property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_BRIGHTNESS
*   - @c #SCREEN_PROPERTY_CHROMATICITY_COORDS
*   - @c #SCREEN_PROPERTY_CONTRAST
*   - @c #SCREEN_PROPERTY_GAMMA [1]
*   - @c #SCREEN_PROPERTY_HUE
*   - @c #SCREEN_PROPERTY_SATURATION
*
*   Note [1]: You must be working within a privileged context of type
*             @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able get set this property.
*
*   @param  disp The handle of the display whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*           <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c float.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_display_property_fv(screen_display_t disp, int pname, float *param);

/**
*   @brief Set the value of the specified display property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a display property from a user-provided buffer.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*
*   @param  disp The handle of the display whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The size (including a terminating null character) of @c param.
*               Screen reads from @c param until it encounters a null character (`\0`), or until
*               @c len number of bytes is reached. Ensure that @c len doesn't exceed the maximum
*               size that Screen allows for the property specified by @c pname.
*   @param  param A pointer to a character array containing the new property value. This array
*                 must have a minimum length of @c len.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_display_property_cv(screen_display_t disp, int pname, int len, const char *param);

/**
*   @brief Set the value of the specified display property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a display property from a user-provided buffer.
*   Typically, you must be working within a privileged context of type
*   @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able to set display-related properties.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_BLUE_GAMMA_CURVE [2]
*   - @c #SCREEN_PROPERTY_BRIGHTNESS
*   - @c #SCREEN_PROPERTY_CBABC_MODE
*   - @c #SCREEN_PROPERTY_CONTRAST
*   - @c #SCREEN_PROPERTY_GAMMA [2]
*   - @c #SCREEN_PROPERTY_GREEN_GAMMA_CURVE [2]
*   - @c #SCREEN_PROPERTY_HUE
*   - @c #SCREEN_PROPERTY_INTENSITY
*   - @c #SCREEN_PROPERTY_MIRROR_MODE
*   - @c #SCREEN_PROPERTY_MODE
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_POSITION
*   - @c #SCREEN_PROPERTY_POWER_MODE [1]
*   - @c #SCREEN_PROPERTY_PROTECTION_ENABLE
*   - @c #SCREEN_PROPERTY_RED_GAMMA_CURVE [2]
*   - @c #SCREEN_PROPERTY_ROTATION
*   - @c #SCREEN_PROPERTY_SATURATION
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR
*   - @c #SCREEN_PROPERTY_VIEWPORT_POSITION
*   - @c #SCREEN_PROPERTY_VIEWPORT_SIZE
*
*   Note [1]: You must be working within a privileged context of type
*             @c #SCREEN_POWER_MANAGER_CONTEXT to be able to set this property.
*
*   Note [2]: You must be working within a privileged context of type
*             @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able get set this property.
*
*   @param  disp The handle of the display whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c int. @c param may be a single integer or
*                 an array of integers depending on the property being set.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_display_property_iv(screen_display_t disp, int pname, const int *param);

/**
*   @brief Set the value of the specified display property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a display property from a user-provided buffer.
*   Typically, you must be working within a privileged context of type
*   @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able to set display-related properties.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_IDLE_TIMEOUT [1]
*
*   Note [1]: You must be working within a privileged context of type
*             @c #SCREEN_POWER_MANAGER_CONTEXT to be able to set this property.
*
*   @param  disp The handle of the display whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_display_property_llv(screen_display_t disp, int pname, const long long *param);

/**
*   @brief Set the value of the specified display property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a display property from a user-provided buffer.
*   Typically, you must be working within a privileged context of type
*   @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able to set display-related properties.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_FOCUS
*
*   @param  disp The handle of the display whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_display_property_pv(screen_display_t disp, int pname, void **param);

/**
*   @brief Set the value of the specified display property of type @c float
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a display property from a user-provided buffer.
*   Typically, you must be working within a privileged context of type
*   @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able to set display-related properties.
*
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_BRIGHTNESS
*   - @c #SCREEN_PROPERTY_CONTRAST
*   - @c #SCREEN_PROPERTY_GAMMA [1]
*   - @c #SCREEN_PROPERTY_HUE
*   - @c #SCREEN_PROPERTY_SATURATION
*
*   Note [1]: You must be working within a privileged context of type
*             @c #SCREEN_DISPLAY_MANAGER_CONTEXT to be able get set this property.
*
*   @param  disp The handle of the display whose property is to be set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s).  This
*                 buffer must be of type @c float.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_display_property_fv(screen_display_t disp, int pname, const float *param);

/**
*   @brief Retrieve the display modes supported by a specified display
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function returns the video modes that are supported by a display. All
*   elements in the list are unique. Note that several modes can have identical
*   resolutions and differ only in refresh rate or aspect ratio. You can obtain
*   the number of modes supported by querying the @c #SCREEN_PROPERTY_MODE_COUNT
*   property. No more than @c max modes is stored.
*
*   @param  display The handle of the display whose display modes are being
*                   queried.
*   @param  max The maximum number of display modes that can be written to the
*               array of modes pointed to by @c param.
*   @param  param The buffer where the retrieved display modes is stored.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_display_modes(screen_display_t display, int max, screen_display_mode_t *param);

/**
*   @brief Take a screenshot of the display and store the resulting image in
*          the specified buffer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function takes a screenshot of a display and stores the result in a
*   user-provided buffer. The buffer can be a pixmap buffer or a window buffer.
*   The buffer must have been created with the usage flag @c #SCREEN_USAGE_NATIVE
*   in order for the operation to succeed. You need to be working within a
*   privileged context so that you have full access to the display properties of
*   the system. Therefore, a context which was created with the type
*   @c #SCREEN_DISPLAY_MANAGER_CONTEXT must be used. When capturing screenshots of
*   multiple displays, you will need to make one @c screen_read_display() function
*   call per display. This function blocks until the operation is completed. If @c count
*   is 0 and @c read_rects is NULL, the entire display is grabbed. Otherwise, set @c count to 1 and
*   @c read_rects must point to the array of integers defining the rectangle and display
*   coordinates that need to be grabbed. Note that the buffer size doesn't have
*   to match the display size. Scaling is applied to make the screenshot fit into the buffer
*   that's provided.
*
*   @param  disp The handle of the display that's the target of the screenshot.
*   @param  buf The buffer where the resulting image is stored.
*   @param  count If @c read_rects is provided, set to 1. Otherwise, set to 0 for a screen shot of the entire display.
*   @param  read_rects A pointer to an array of integers {x,y,w,h}, containing the x,y
*           coordinates of the top left corner, width, and height of the rectangle respectively to capture.
*   @param  flags For future use. Pass 0 for now.
*
*   @return @c 0 if successful, or @c -1 if an error occurred. @c errno is set; refer to @c errno.h for more details.
*           Possible error codes include:
*          - @c ENOTSUP: The display is mirrored
*/
int screen_read_display(screen_display_t disp, screen_buffer_t buf, int count, const int *read_rects, int flags);

/**
*   @brief Block the calling thread until the next vsync happens on the
*          specified display
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function blocks the calling thread and returns when the next vsync
*   operation occurs on the specified display.
*
*   @param  display An instance of the display on which to perform the vsync operation.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_wait_vsync(screen_display_t display);

/**
*   @brief Create a virtual display
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a virtual display as specified by the provided mode. If the @c SCREEN_DISPLAY_MODE_FORMAT_VALID
*   flag is set, then @c mode.format is used. You need to set both the @c mode.width and @c mode.height in your @c mode structure.
*
*
*   @param  pdisp A pointer to the address where the handle for the created display can be stored.
*   @param  cxt The handle of the context in which the virtual display is to be
*               created. This context must have been created with the context type
*               of @c SCREEN_DISPLAY_MANAGER_CONTEXT or @c SCREEN_WINDOW_MANAGER_CONTEXT
*               using @c screen_create_context().
*   @param  mode A pointer to the buffer that contains the display mode.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_display(screen_display_t *pdisp, screen_context_t cxt, screen_display_mode_t *mode);

/**
*   @brief Destroy a virtual display
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function destroys the virtual display associated with the display handle. The display handle can no longer be used
*   in subsequent Screen API calls. Any resources created for the display are released.
*   Applications are responsible for freeing their own external buffers.
*
*   @param  disp The handle of the virtual display to be destroyed. The virtual display must have
*                been created using @c SCREEN_CREATE_DISPLAY().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_display(screen_display_t disp);

/** @} */   /* end of ingroup screen_displays */

/**
*   @ingroup screen_events
*   @{
*/
/**
*   @brief Create an event that can later be filled with event data
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates an event object. This event can be used to store
*   events from the event queue of the process by using @c screen_get_event(). Event data
*   can also be filled in with the @c screen_set_event_property() functions and
*   sent to other applications using @c screen_inject_event() or
*   @c screen_send_event(). Events are opaque handles. The @c screen_get_event_property_*()
*   functions must be used to get information from the event. You must destroy
*   event objects when you no longer need them by using @c screen_destroy_event().
*
*   @param  pev An address where the function can store a handle to the native
*               event.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_event(screen_event_t *pev);

/**
*   @brief Destroy an event and free associated memory
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function frees the memory allocated to hold an event. The event can no
*   longer be used as an argument in subsequent Screen API calls.
*
*   @param  ev The handle of the event to destroy. This event must have been
*              created with @c screen_create_event().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_event(screen_event_t ev);

/**
*   @brief Wait for a screen event
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>

*   This function gets the next event associated with the given context. If no
*   events have been queued, the function waits up to the specified amount
*   of time for an event to occur. If the function times out before an event
*   becomes available, an event with a @c #SCREEN_EVENT_NONE type is returned.
*
*   @param  ctx The context to retrieve events from. This context must have been
*               created using @c screen_create_context().
*   @param  ev An event previously allocated with @c screen_create_event(). Any
*              contents in this event is replaced with the next event.
*   @param  timeout The maximum time (in nanoseconds) to wait for an event to occur, if one has
*                   not been queued up already. @c 0 indicates that the call must not
*                   wait at all if there are no events associated with the specified
*                   context. @c -1 indicates that the call must not return until an
*                   event is ready.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_event(screen_context_t ctx, screen_event_t ev, uint64_t timeout);

/**
*   @brief Retrieve the current value of the specified event property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function stores the current value of an event property in a
*   user-provided buffer. The list of properties that can be queried per event
*   type are listed as follows:
*
*   Event Type: @c #SCREEN_EVENT_CREATE
*   - @c #SCREEN_PROPERTY_GROUP
*
*   Event Type: @c #SCREEN_EVENT_MANAGER
*   - @c #SCREEN_PROPERTY_USER_DATA
*
*   @param  ev The handle of the event whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The maximum number of bytes that Screen can write to @c param.
*   @param  param The buffer where Screen stores the retrieved value (a character array that
*                 includes a terminating null character). This buffer must be an array of type
*                 @c char with a minimum length of @c len. If this user-provided buffer has
*                 insufficient space for storing the property being retrieved, Screen
*                 truncates the retrieved value and returns the character array without a
*                 terminating null character.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_event_property_cv(screen_event_t ev, int pname, int len, char *param);

/**
*   @brief Retrieve the current value of the specified event property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function stores the current value of an event property in a user-provided buffer.
*   The list of properties that can be queried per event type are listed as follows:
*
*   Event Type: Any
*   - @c #SCREEN_PROPERTY_OBJECT_TYPE
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR
*   - @c #SCREEN_PROPERTY_TYPE
*
*   Event Type: @c #SCREEN_EVENT_DISPLAY
*   - @c #SCREEN_PROPERTY_ATTACHED
*   - @c #SCREEN_PROPERTY_MIRROR_MODE
*   - @c #SCREEN_PROPERTY_MODE
*   - @c #SCREEN_PROPERTY_PROTECTION_ENABLE
*
*   Event Type: @c #SCREEN_EVENT_GAMEPAD
*   - @c #SCREEN_PROPERTY_ANALOG0
*   - @c #SCREEN_PROPERTY_ANALOG1
*   - @c #SCREEN_PROPERTY_BUTTONS
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_MODIFIERS
*
*   Event Type: @c #SCREEN_EVENT_GESTURE
*   - @c #SCREEN_PROPERTY_BUTTONS
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_DISPLACEMENT
*   - @c #SCREEN_PROPERTY_FLAGS
*   - @c #SCREEN_PROPERTY_MODIFIERS
*   - @c #SCREEN_PROPERTY_POSITION
*   - @c #SCREEN_PROPERTY_SCAN
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_SIZE
*   - @c #SCREEN_PROPERTY_SYM
*
*   Event Type: @c #SCREEN_EVENT_IDLE
*   - @c #SCREEN_PROPERTY_IDLE_STATE
*
*   Event Type: @c #SCREEN_EVENT_INPUT
*    - @c #SCREEN_PROPERTY_DEVICE
*    - @c #SCREEN_PROPERTY_INPUT_VALUE
*
*  Event Type: @c #SCREEN_EVENT_INPUT_CONTROL
*    - @c #SCREEN_PROPERTY_BUTTONS (only for events with name @c #SCREEN_INPUT_CONTROL_POINTER_START
*      or @c #SCREEN_INPUT_CONTROL_POINTER_STOP)
*    - @c #SCREEN_PROPERTY_DEVICE
*    - @c #SCREEN_PROPERTY_NAME
*    - @c #SCREEN_PROPERTY_TOUCH_ID (only for events with name @c #SCREEN_INPUT_CONTROL_MTOUCH_START
*      or @c #SCREEN_INPUT_CONTROL_MTOUCH_STOP)
*
*   Event Type: @c #SCREEN_EVENT_JOG
*    - @c #SCREEN_PROPERTY_DEVICE
*    - @c #SCREEN_PROPERTY_JOG_COUNT
*
*   Event Type: @c #SCREEN_EVENT_JOYSTICK
*   - @c #SCREEN_PROPERTY_ANALOG0
*   - @c #SCREEN_PROPERTY_BUTTONS
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_MODIFIERS
*
*   Event Type: @c #SCREEN_EVENT_KEYBOARD
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_FLAGS
*   - @c #SCREEN_PROPERTY_KEY_ALTERNATE_SYM
*   - @c #SCREEN_PROPERTY_KEY_CAP
*   - @c #SCREEN_PROPERTY_MODIFIERS
*   - @c #SCREEN_PROPERTY_SCAN
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_SYM
*
*   Event Type: @c #SCREEN_EVENT_MANAGER
*   - @c #SCREEN_PROPERTY_SIZE
*   - @c #SCREEN_PROPERTY_SUBTYPE
*
*   Event Types: @c #SCREEN_EVENT_MTOUCH_PRETOUCH, @c #SCREEN_EVENT_MTOUCH_TOUCH,
*                @c #SCREEN_EVENT_MTOUCH_MOVE, @c #SCREEN_EVENT_MTOUCH_RELEASE
*   - @c #SCREEN_PROPERTY_BUTTONS
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_MODIFIERS
*   - @c #SCREEN_PROPERTY_POSITION
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_SIZE
*   - @c #SCREEN_PROPERTY_SOURCE_POSITION
*   - @c #SCREEN_PROPERTY_SOURCE_SIZE
*   - @c #SCREEN_PROPERTY_TOUCH_ID
*   - @c #SCREEN_PROPERTY_TOUCH_ORIENTATION
*   - @c #SCREEN_PROPERTY_TOUCH_PRESSURE
*   - @c #SCREEN_PROPERTY_TOUCH_TYPE
*
*   Event Type: @c #SCREEN_EVENT_POINTER
*   - @c #SCREEN_PROPERTY_BUTTONS
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_MODIFIERS
*   - @c #SCREEN_PROPERTY_MOUSE_HORIZONTAL_WHEEL
*   - @c #SCREEN_PROPERTY_MOUSE_WHEEL
*   - @c #SCREEN_PROPERTY_POSITION
*   - @c #SCREEN_PROPERTY_SOURCE_POSITION
*
*   Event Type: @c #SCREEN_EVENT_PROPERTY
*    - @c #SCREEN_PROPERTY_NAME
*
*   Event Type: @c #SCREEN_EVENT_USER
*    - @c #SCREEN_PROPERTY_USER_DATA
*
*   @param  ev The handle of the event whose property is being queried. The
*              event must have an event type of
*              <a href="group__screen__events_1Screen_Event_Types.xml">Screen event types</a>.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c int. The buffer may be a single integer or
*                 an array of integers, depending on the property being retrieved.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_event_property_iv(screen_event_t ev, int pname, int *param);

/**
*   @brief Retrieve the current value of the specified event property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function stores the current value of an event property in a user-provided buffer.
*
*   Event Type: Any
*    - @c #SCREEN_PROPERTY_DURATION (for @c #SCREEN_EVENT_GESTURE only)
*    - @c #SCREEN_PROPERTY_TIMESTAMP
*
*   @param  ev The handle of the event whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of @c long @c long integer.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_event_property_llv(screen_event_t ev, int pname, long long *param);

/**
*   @brief Retrieve the current value of the specified event property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function stores the current value of an event property in a
*   user-provided buffer. The list of properties that can be queried per event
*   type are listed as follows:
*
*   Event Type: Any
*   - @c #SCREEN_PROPERTY_CONTEXT
*
*   Event Type: @c #SCREEN_EVENT_CLOSE
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_GROUP
*   - @c #SCREEN_PROPERTY_PIXMAP
*   - @c #SCREEN_PROPERTY_SESSION
*   - @c #SCREEN_PROPERTY_STREAM
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_CREATE
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_GROUP
*   - @c #SCREEN_PROPERTY_PIXMAP
*   - @c #SCREEN_PROPERTY_SESSION
*   - @c #SCREEN_PROPERTY_STREAM
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_DISPLAY
*   - @c #SCREEN_PROPERTY_DISPLAY
*
*   Event Type: @c #SCREEN_EVENT_GAMEPAD
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_IDLE
*    - @c #SCREEN_PROPERTY_DISPLAY
*    - @c #SCREEN_PROPERTY_GROUP
*
*   Event Type: @c #SCREEN_EVENT_INPUT
*    - @c #SCREEN_PROPERTY_DEVICE
*
*   Event Type: @c #SCREEN_EVENT_INPUT_CONTROL
*    - @c #SCREEN_PROPERTY_DEVICE
*    - @c #SCREEN_PROPERTY_SESSION
*    - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_JOG
*   - @c #SCREEN_PROPERTY_DEVICE
*
*   Event Type: @c #SCREEN_EVENT_JOYSTICK
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_KEYBOARD
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_MANAGER
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Types: @c #SCREEN_EVENT_MTOUCH_PRETOUCH, @c #SCREEN_EVENT_MTOUCH_TOUCH,
*                @c #SCREEN_EVENT_MTOUCH_MOVE, @c #SCREEN_EVENT_MTOUCH_RELEASE
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_POINTER
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_POST
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_PROPERTY
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_GROUP
*   - @c #SCREEN_PROPERTY_PIXMAP
*   - @c #SCREEN_PROPERTY_SESSION
*   - @c #SCREEN_PROPERTY_STREAM
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_UNREALIZE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Note that when you're retrieving the following properties, Screen locally allocates resources to track
*   the object that you retrieve:
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_GROUP
*   - @c #SCREEN_PROPERTY_PIXMAP
*   - @c #SCREEN_PROPERTY_SESSION
*   - @c #SCREEN_PROPERTY_STREAM
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Therefore, you must call the appropriate screen_destroy_*() function to free these resources
*   when you no longer need to use the object in your local context.
*
*   @param  ev The handle of the event whose property is being queried. The
*              event must have an event type of
*              <a href="group__screen__events_1Screen_Event_Types.xml">Screen event types</a>.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_event_property_pv(screen_event_t ev, int pname, void **param);

/**
*   @brief Send an input event to the window that has input focus on a given display
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   A window manager and an input provider can use this function when they need
*   to inject an event in the system. You need to be within a privileged context
*   to be able to inject input events. You can create a privileged context by
*   calling the function @c screen_create_context() with a context type of
*   @c #SCREEN_WINDOW_MANAGER_CONTEXT or @c #SCREEN_INPUT_PROVIDER_CONTEXT.
*   Prior to calling @c screen_inject_event(), you must have set all relevant event
*   properties to valid values, especially the event type property.
*   This function sends the event to the window that has input focus on the specified display. If
*   you want to send an event to a particular window other than the one who has input focus, then
*   use @c screen_send_event().
*
*   @param  disp The display into which the event is injected. You can
*                obtain a handle to the display by either @c screen_get_context_property()
*                or @c screen_get_window_property() functions.
*   @param  ev An event handle that was created with @c screen_create_event(). This
*              event must contain all the relevant event data pertaining to its type
*              when injected into the system.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_inject_event(screen_display_t disp, screen_event_t ev);

/**
*   @brief Send an input event to a process
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   A window manager and an input provider can use this function when they need
*   to inject an event in the system. Prior to calling @c screen_send_event(), you
*   must have set all relevant event properties to valid values, especially the
*   event type property. To send input events, you must be
*   - within the process identified by @c pid or
*   - using a context with the @c #SCREEN_WINDOW_MANAGER_CONTEXT or @c #SCREEN_INPUT_PROVIDER_CONTEXT flag.
*
*   @param  ctx A context within Screen that was created with
*               @c screen_create_context().
*   @param  ev An event handle that was created with @c screen_create_event(). This
*              event must contain all the relevant event data pertaining to its type
*              when injected into the system.
*   @param  pid The process the event is to be sent to.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_send_event(screen_context_t ctx, screen_event_t ev, pid_t pid);

/**
*   @brief Set the value of the specified event property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function sets the value of an event property from a user-provided buffer.
*   The list of properties that can be set per event type are listed as follows:
*
*   Event Type: @c #SCREEN_EVENT_CREATE
*   - @c #SCREEN_PROPERTY_GROUP
*
*   Event Type: @c #SCREEN_EVENT_MANAGER
*    - @c #SCREEN_PROPERTY_USER_DATA
*
*   @param  ev The handle of the event whose property is being set. Ensure that you've set the
*              @c SCREEN_PROPERTY_TYPE to one of the valid <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>
*              prior to calling this function.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The size (including a terminating null character) of @c param.
*               Screen reads from @c param until it encounters a null character (`\0`), or until
*               @c len number of bytes is reached. Ensure that @c len doesn't exceed the maximum
*               size that Screen allows for the property specified by @c pname.
*   @param  param A pointer to a character array containing the new property value. This array
*                 must have a minimum length of @c len.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_event_property_cv(screen_event_t ev, int pname, int len, const char *param);

/**
*   @brief Set the value of the specified event property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function sets the value of an event property from a user-provided buffer.
*   The list of properties that can be set per event type are listed as follows:
*
*   Event Type: Any
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR
*   - @c #SCREEN_PROPERTY_TYPE
*
*   Event Type: @c #SCREEN_EVENT_DISPLAY
*   - @c #SCREEN_PROPERTY_ATTACHED
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_MIRROR_MODE
*   - @c #SCREEN_PROPERTY_MODE
*   - @c #SCREEN_PROPERTY_PROTECTION_ENABLE
*
*   Event Type: @c #SCREEN_EVENT_GAMEPAD
*   - @c #SCREEN_PROPERTY_ANALOG0
*   - @c #SCREEN_PROPERTY_ANALOG1
*   - @c #SCREEN_PROPERTY_BUTTONS
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_MODIFIERS
*
*   Event Type: @c #SCREEN_EVENT_IDLE
*    - @c #SCREEN_PROPERTY_IDLE_STATE
*
*   Event Type: @c #SCREEN_EVENT_INPUT
*    - @c #SCREEN_PROPERTY_DEVICE
*    - @c #SCREEN_PROPERTY_INPUT_VALUE
*
*   Event Type: @c #SCREEN_EVENT_INPUT_CONTROL
*   - @c #SCREEN_PROPERTY_BUTTONS
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_NAME
*   - @c #SCREEN_PROPERTY_TOUCH_ID
*
*   Event Type: @c #SCREEN_EVENT_JOG
*    - @c #SCREEN_PROPERTY_DEVICE
*    - @c #SCREEN_PROPERTY_JOG_COUNT
*
*   Event Type: @c #SCREEN_EVENT_JOYSTICK
*   - @c #SCREEN_PROPERTY_ANALOG0
*   - @c #SCREEN_PROPERTY_BUTTONS
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_MODIFIERS
*
*   Event Type: @c #SCREEN_EVENT_KEYBOARD
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_KEY_ALTERNATE_SYM
*   - @c #SCREEN_PROPERTY_KEY_CAP
*   - @c #SCREEN_PROPERTY_FLAGS
*   - @c #SCREEN_PROPERTY_MODIFIERS
*   - @c #SCREEN_PROPERTY_SCAN
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_SYM
*
*   Event Type: @c #SCREEN_EVENT_MANAGER
*    - @c #SCREEN_PROPERTY_SUBTYPE
*
*   Event Types: @c #SCREEN_EVENT_MTOUCH_PRETOUCH, @c #SCREEN_EVENT_MTOUCH_TOUCH,
*                @c #SCREEN_EVENT_MTOUCH_MOVE, @c #SCREEN_EVENT_MTOUCH_RELEASE
*   - @c #SCREEN_PROPERTY_BUTTONS
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_MODIFIERS
*   - @c #SCREEN_PROPERTY_POSITION
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_SIZE
*   - @c #SCREEN_PROPERTY_SOURCE_POSITION
*   - @c #SCREEN_PROPERTY_SOURCE_SIZE
*   - @c #SCREEN_PROPERTY_TOUCH_ID
*   - @c #SCREEN_PROPERTY_TOUCH_ORIENTATION
*   - @c #SCREEN_PROPERTY_TOUCH_PRESSURE
*   - @c #SCREEN_PROPERTY_TOUCH_TYPE
*
*   Event Type: @c #SCREEN_EVENT_POINTER
*   - @c #SCREEN_PROPERTY_BUTTONS
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_MODIFIERS
*   - @c #SCREEN_PROPERTY_MOUSE_HORIZONTAL_WHEEL
*   - @c #SCREEN_PROPERTY_MOUSE_WHEEL
*   - @c #SCREEN_PROPERTY_POSITION
*   - @c #SCREEN_PROPERTY_SOURCE_POSITION
*
*   Event Type: @c #SCREEN_EVENT_PROPERTY
*    - @c #SCREEN_PROPERTY_NAME
*
*   Event Type: @c #SCREEN_EVENT_USER
*    - @c #SCREEN_PROPERTY_USER_DATA
*
*   @param  ev The handle of the event whose property is being set. Ensure that you've set the
*              @c SCREEN_PROPERTY_TYPE to one of the valid <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>
*              prior to calling this function.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c int. @c param may be a single integer or
*                 an array of integers depending on the property being set.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_event_property_iv(screen_event_t ev, int pname, const int *param);

/**
*   @brief Set the current value of the specified event property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function sets the value of an event property from a user-provided array.
*   The list of properties that can be set per event type are listed as follows:
*   - @c #SCREEN_PROPERTY_DURATION (for @c #SCREEN_EVENT_GESTURE only)
*   - @c #SCREEN_PROPERTY_TIMESTAMP
*
*   @param  ev The handle of the event whose property is being set. Ensure that you've set the
*              @c SCREEN_PROPERTY_TYPE to one of the valid <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>
*              prior to calling this function.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_event_property_llv(screen_event_t ev, int pname, const long long *param);

/**
*   @brief Set the value of the specified event property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function sets the value of an event property from a user-provided array.
*   The list of properties that can be set per event type are listed as follows:
*
*   Event Type: Any
*   - @c #SCREEN_PROPERTY_CONTEXT
*
*   Event Type: @c #SCREEN_EVENT_CREATE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_DISPLAY
*   - @c #SCREEN_PROPERTY_DISPLAY
*
*   Event Type: @c #SCREEN_EVENT_GAMEPAD
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_IDLE
*    - @c #SCREEN_PROPERTY_DISPLAY
*    - @c #SCREEN_PROPERTY_GROUP
*
*   Event Type: @c #SCREEN_EVENT_INPUT
*    - @c #SCREEN_PROPERTY_DEVICE
*
*   Event Type: @c #SCREEN_EVENT_INPUT_CONTROL
*    - @c #SCREEN_PROPERTY_DEVICE
*    - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_JOG
*   - @c #SCREEN_PROPERTY_DEVICE
*
*   Event Type: @c #SCREEN_EVENT_JOYSTICK
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_KEYBOARD
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_MANAGER
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Types: @c #SCREEN_EVENT_MTOUCH_PRETOUCH, @c #SCREEN_EVENT_MTOUCH_TOUCH,
*                @c #SCREEN_EVENT_MTOUCH_MOVE, @c #SCREEN_EVENT_MTOUCH_RELEASE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_POINTER
*   - @c #SCREEN_PROPERTY_DEVICE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_POST
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_PROPERTY
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_GROUP
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   Event Type: @c #SCREEN_EVENT_UNREALIZE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   @param  ev The handle of the event whose property is being set. Ensure that you've set the
*              @c SCREEN_PROPERTY_TYPE to one of the valid <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>
*              prior to calling this function.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_event_property_pv(screen_event_t ev, int pname, void **param);
/** @} */   /* end of ingroup screen_events */

/*
 * Groups
 */
/**
*   @ingroup screen_groups
*   @{
*/
/**
*   @brief Create a group
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a group that windows or pixmaps can join. You can use groups in order
*   to organize your application windows and pixmaps. Use screen_join_window_group() and
*   screen_join_pixmap_group() to join a window or pixmap respectively to a group.
*
*   Provide the name of this group (retrieved by calling screen_get_group_property_cv()
*   the #SCREEN_PROPERTY_NAME) to windows and pixmaps that need to join it.
*
*   Call screen_destroy_group() when you no longer need the group.
*
*   @param  pgrp The handle of the group.
*   @param  ctx The connection to Screen. This context must
*               have been created with @c screen_create_context().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_group(screen_group_t *pgrp, screen_context_t ctx);

/**
*   @brief Destroy a group
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function destroys a group given a @c #screen_group_t instance. When
*   a group is destroyed, all windows and pixmaps that belonged to the group are no
*   longer associated with the group. You must destroy each @c #screen_group_t
*   after it's no longer needed.
*
*   @param  grp The group to be destroyed. The group must have been
*               created with @c screen_create_group() or @c screen_create_window_group().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_group(screen_group_t grp);

/**
*   @brief Retrieve the current value of the specified group property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of group property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_ID
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_NAME
*
*   @param  grp The handle of the group whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The maximum number of bytes that Screen can write to @c param.
*   @param  param The buffer where Screen stores the retrieved value (a character array that
*                 includes a terminating null character). This buffer must be an array of type
*                 @c char with a minimum length of @c len. If this user-provided buffer has
*                 insufficient space for storing the property being retrieved, Screen
*                 truncates the retrieved value and returns the character array without a
*                 terminating null character.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_group_property_cv(screen_group_t grp, int pname, int len, char *param);

/**
*   @brief Retrieve the current value of the specified group property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of group property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_ID
*   - @c #SCREEN_PROPERTY_IDLE_STATE
*   - @c #SCREEN_PROPERTY_KEEP_AWAKES
*   - @c #SCREEN_PROPERTY_OWNER_PID
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_STATUS
*
*   @param  grp The handle of the group whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c int. The buffer may be a single integer or
*                 an array of integers, depending on the property being retrieved.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_group_property_iv(screen_group_t grp, int pname, int *param);

/**
*   @brief Retrieve the current value of the specified group property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of group property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_IDLE_TIMEOUT
*
*   @param  grp The handle of the group whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_group_property_llv(screen_group_t grp, int pname, long long *param);

/**
*   @brief Retrieve the current value of the specified group property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of group property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_CONTEXT
*   - @c #SCREEN_PROPERTY_FOCUS
*   - @c #SCREEN_PROPERTY_USER_HANDLE
*
*   @param  grp The handle of the group whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_group_property_pv(screen_group_t grp, int pname, void **param);

/**
*   @brief Set the value of the specified group property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a group property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_NAME: Lower case and special characters are permitted. However, the name
*                               must be unique within your system and must not start with <tt>__scrn-</tt>.
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*
*   @param  grp The handle of the group whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The size (that includes a terminating null character) of @c param.
*               Screen reads from @c param until it encounters a null character (`\0`), or until
*               @c len number of bytes is reached. Ensure that @c len doesn't exceed the maximum
*               size that Screen allows for the property specified by @c pname.
*   @param  param A pointer to a character array containing the new property value. This array
*                 must have a minimum length of @c len.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_group_property_cv(screen_group_t grp, int pname, int len, const char *param);

/**
*   @brief Set the value of the specified group property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a group property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*
*   @param  grp The handle of the group whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c int. @c param may be a single integer or
*                 an array of integers depending on the property being set.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_group_property_iv(screen_group_t grp, int pname, const int *param);

/**
*   @brief Set the value of the specified group property of type @c long @c long
*          integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a group property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_IDLE_TIMEOUT
*
*   @param  grp The handle of the group whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_group_property_llv(screen_group_t grp, int pname, const long long *param);

/**
*   @brief Set the value of the specified group property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a group property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_FOCUS
*   - @c #SCREEN_PROPERTY_USER_HANDLE
*
*   @param  grp The handle of the group whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_group_property_pv(screen_group_t grp, int pname, void **param);
/** @} */   /* end of ingroup screen_groups */

/*
 * Pixmaps
 */
/**
*   @ingroup screen_pixmaps
*   @{
*/
/**
*   @brief Associate a buffer with a pixmap
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   You must be working within a @c #SCREEN_BUFFER_PROVIDER_CONTEXT in order to call this function successfully.
*   This function allows a pixmap to use a buffer that was allocated by the application or Screen.
*   Note that pixmaps can have only one buffer. For more information, see the <a href="manual/cscreen_buffers_create-attach.xml"> "Buffers" </a>
*   chapter of the <em>Screen Developer's Guide</em>.
*
*   @param  pix The handle of a pixmap that the buffer is being attached to.
*   @param  buf A buffer that's to be attached to @c pix.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_attach_pixmap_buffer(screen_pixmap_t pix, screen_buffer_t buf);

/**
*   @brief Create a pixmap that can be used to do off-screen rendering
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a pixmap object, which is an off-screen rendering
*   target. The results of this rendering can later be copied to a window object.
*   Applications must use @c screen_destroy_pixmap() when a pixmap is no longer
*   used.
*
*   @param  ppix An address where the function can store the handle to the
*                newly created native pixmap.
*   @param  ctx The connection to Screen. This context must have been created
*               with @c screen_create_context().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_pixmap(screen_pixmap_t *ppix, screen_context_t ctx);

/**
*   @brief Send a request to Screen to create a new buffer for a pixmap
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function creates a buffer for a pixmap.
*
*   Before calling this function, ensure that you set:
*   - @c #SCREEN_PROPERTY_BUFFER_SIZE
*   - @c #SCREEN_PROPERTY_FORMAT
*   - @c #SCREEN_PROPERTY_USAGE property on the pixmap (@c pix)
*     Setting the usage indicates your intended usage of the pixmap's buffer.
*     For example, to retrieve @c #SCREEN_PROPERTY_POINTER from the buffer, you must have set
*     the @c #SCREEN_PROPERTY_USAGE property to include the #SCREEN_USAGE_READ and/or
*     @c #SCREEN_USAGE_WRITE flags on the pixmap prior to calling screen_create_pixmap_buffer().
*
*   @param  pix The handle of the pixmap for which a new buffer is created.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_pixmap_buffer(screen_pixmap_t pix);

/**
*   @brief Destroy a pixmap and frees associated resources
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function destroys the pixmap associated with the specified pixmap. Any
*   resources and buffer created for this pixmap, whether locally or by Screen
*   is released. The pixmap handle can no longer be used as an argument in subsequent Screen API calls.
*   Pixmap buffers that aren't created by Screen, but are registered with
*   @c screen_attach_pixmap_buffer() aren't freed by this operation. The
*   application is responsible for freeing its own external buffers.
*
*   @param  pix The handle of the pixmap which is to be destroyed.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_pixmap(screen_pixmap_t pix);

/**
*   @brief Send a request to Screen to destroy the buffer of the specified pixmap
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function releases the buffer that was allocated for a pixmap, without
*   destroying the pixmap. If the buffer was created with
*   @c screen_create_pixmap_buffer(), the memory is released and can be used for
*   other window or pixmap buffers. If the buffer was attached using
*   @c screen_attach_pixmap_buffer(), the buffer is destroyed but no memory is
*   actually released. In this case the application is responsible for freeing
*   the memory after calling @c screen_destroy_pixmap_buffer(). Once a pixmap
*   buffer has been destroyed, you can change the format, usage and buffer size
*   before creating a new buffer again.
*   The memory that's released by this call is not reserved and can be used for
*   any subsequent buffer allocation by Screen.
*
*   @param  pix The handle of the pixmap whose buffer is to be destroyed.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_pixmap_buffer(screen_pixmap_t pix);

/**
*   @brief Retrieve the current value of the specified pixmap property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value of pixmap property from a user-provided buffer.
*   The values of the following properties can be queried using this function:
*   - @c #SCREEN_PROPERTY_GROUP
*   - @c #SCREEN_PROPERTY_ID
*   - @c #SCREEN_PROPERTY_ID_STRING
*
*   @param  pix The handle of the pixmap whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The maximum number of bytes that Screen can write to @c param.
*   @param  param The buffer where Screen stores the retrieved value (a character array that
*                 includes a terminating null character). This buffer must be an array of type
*                 @c char with a minimum length of @c len. If this user-provided buffer has
*                 insufficient space for storing the property being retrieved, Screen
*                 truncates the retrieved value and returns the character array without a
*                 terminating null character.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_pixmap_property_cv(screen_pixmap_t pix, int pname, int len, char *param);

/**
*   @brief Retrieve the current value of the specified pixmap property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value of pixmap property from a user-provided buffer.
*   The values of the following properties can be queried using this function:
*   - @c #SCREEN_PROPERTY_ALPHA_MODE
*   - @c #SCREEN_PROPERTY_BUFFER_SIZE
*   - @c #SCREEN_PROPERTY_COLOR_SPACE
*   - @c #SCREEN_PROPERTY_FORMAT
*   - @c #SCREEN_PROPERTY_ID [1]
*   - @c #SCREEN_PROPERTY_METRIC_COUNT [1]
*   - @c #SCREEN_PROPERTY_OWNER_PID [1]
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_STATUS
*   - @c #SCREEN_PROPERTY_USAGE
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   @param  pix The handle of the pixmap whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c int. The buffer may be a single integer or
*                 an array of integers, depending on the property being retrieved.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_pixmap_property_iv(screen_pixmap_t pix, int pname, int *param);

/**
*   @brief Retrieve the current value of the specified pixmap property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value of pixmap property from a user-provided buffer.
*   The values of the following properties can be queried using this function:
*    - @c #SCREEN_PROPERTY_METRICS
*
*   @param  pix The handle of the pixmap whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_pixmap_property_llv(screen_pixmap_t pix, int pname, long long *param);

/**
*   @brief Retrieve the current value of the specified pixmap property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value of pixmap property from a user-provided buffer.
*   The values of the following properties can be queried using this function:
*   - @c #SCREEN_PROPERTY_BUFFERS
*   - @c #SCREEN_PROPERTY_CONTEXT [1]
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_GROUP [2]
*   - @c #SCREEN_PROPERTY_RENDER_BUFFERS [1]
*   - @c #SCREEN_PROPERTY_USER_HANDLE [1]
*
*   Note [1]: When you're retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   Note [2]: When you're retrieving this property, Screen locally allocates resources to track the
*             group that you retrieve. Therefore, you must call screen_destroy_group() to free
*             these resources when you no longer need to use the group in your local context.
*
*   @param  pix The handle of the pixmap whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_pixmap_property_pv(screen_pixmap_t pix, int pname, void **param);

/**
*   @brief Cause a pixmap to join a group
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function is used to add a pixmap to a group.
*
*   @param  pix The handle of the pixmap that is to be joining the group.
*   @param  name A unique string that identifies the group. This is the #SCREEN_PROPERTY_NAME
*                property of the group that the specified pixmap is joining.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_join_pixmap_group(screen_pixmap_t pix, const char *name);

/**
*   @brief Cause a pixmap to leave a group
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function is used to remove a pixmap from a group.
*
*   @param  pix The handle of the pixmap that is to be leaving the group.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_leave_pixmap_group(screen_pixmap_t pix);

/**
*   @brief Create a reference to a pixmap
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a reference to a pixmap. This function can be used
*   by libraries to prevent the pixmap or its buffer from disappearing while
*   the library is making use of it. The pixmap and its buffer will not be
*   destroyed until all references have been cleared with
*   @c screen_unref_pixmap(). In the event that a pixmap is destroyed before the
*   reference is cleared, @c screen_unref_pixmap() will cause the pixmap buffer
*   and/or the pixmap to be destroyed.
*
*   @param  pix The handle of the pixmap for which the reference is to be
*               created.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_ref_pixmap(screen_pixmap_t pix);

/**
*   @brief Set the value of the specified pixmap property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a pixmap property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*
*   @param  pix handle of the pixmap whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The size (that includes a terminating null character) of @c param.
*               Screen reads from @c param until it encounters a null character (`\0`), or until
*               @c len number of bytes is reached. Ensure that @c len doesn't exceed the maximum
*               size that Screen allows for the property specified by @c pname.
*   @param  param A pointer to a character array containing the new property value. This array
*                 must have a minimum length of @c len.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_pixmap_property_cv(screen_pixmap_t pix, int pname, int len, const char *param);

/**
*   @brief Set the value of the specified pixmap property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a pixmap property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ALPHA_MODE
*   - @c #SCREEN_PROPERTY_BUFFER_SIZE
*   - @c #SCREEN_PROPERTY_COLOR_SPACE
*   - @c #SCREEN_PROPERTY_FORMAT
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_USAGE
*
*   @param  pix handle of the pixmap whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c int. @c param may be a single integer or
*                 an array of integers depending on the property being set.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_pixmap_property_iv(screen_pixmap_t pix, int pname, const int *param);

/**
*   @brief Set the value of the specified pixmap property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a pixmap property from a user-provided buffer.
*   Currently, there are no pixmap properties that can be set using this
*   function.
*
*   @param  pix handle of the pixmap whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_pixmap_property_llv(screen_pixmap_t pix, int pname, const long long *param);

/**
*   @brief Set the value of the specified pixmap property of type void*

*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a pixmap property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_DISPLAY
*
*   @param  pix handle of the pixmap whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_pixmap_property_pv(screen_pixmap_t pix, int pname, void **param);

/**
*   @brief Remove a reference from a specified pixmap
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function removes a reference to a pixmap. If the pixmap and its
*   buffer haven't been destroyed yet, the effect of @c screen_unref_pixmap() is
*   simply to decrease a reference count. If the pixmap or the pixmap buffer
*   was destroyed while still being referenced, @c screen_unref_pixmap() will
*   cause the pixmap and/or its buffer to be destroyed when the reference
*   count reaches zero.
*
*   @param  pix The handle of the pixmap for which the reference is to be
*               removed.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_unref_pixmap(screen_pixmap_t pix);
/** @} */   /* end of ingroup screen_pixmaps */

/*
 * Sessions
 */
/**
*   @ingroup screen_sessions
*   @{
*/
/**
*   @brief Create a session of specified type to be associated with a context
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   The @c screen_create_session_type() function creates an input session object to
*   be associated with a context. Applications must use @c screen_destroy_session()
*   when a session is no longer active.
*
*   The following are valid input session types which can be created:
*   - @c #SCREEN_EVENT_GAMEPAD
*   - @c #SCREEN_EVENT_GESTURE
*   - @c #SCREEN_EVENT_JOYSTICK
*   - @c #SCREEN_EVENT_JOG
*   - @c #SCREEN_EVENT_KEYBOARD
*   - @c #SCREEN_EVENT_MTOUCH_TOUCH
*   - @c #SCREEN_EVENT_POINTER
*
*   @param  pssn A pointer to a @c #screen_session_t where a handle for the new
*                input session can be stored.
*   @param  ctx The handle of the context in which the input session is to be
*               created.
*   @param  type The type of input session to be created.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_session_type(screen_session_t *pssn, screen_context_t ctx, int type);

/**
*   @brief Destroy a session
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function destroys a session given a @c #screen_session_t instance.
*   You must destroy each @c #screen_session_t after it is no longer needed.
*
*   @param  ssn The session to be destroyed. The session must have been
*               created with @c screen_create_session_type().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_session(screen_session_t ssn);

/**
*   @brief Retrieve the current value of the specified session property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a session property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_ID
*   - @c #SCREEN_PROPERTY_ID_STRING
*
*   @param  ssn The handle of the session whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The maximum number of bytes that Screen can write to @c param.
*   @param  param The buffer where Screen stores the retrieved value (a character array that
*                 includes a terminating null character). This buffer must be an array of type
*                 @c char with a minimum length of @c len. If this user-provided buffer has
*                 insufficient space for storing the property being retrieved, Screen
*                 truncates the retrieved value and returns the character array without a
*                 terminating null character.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_session_property_cv(screen_session_t ssn, int pname, int len, char *param);

/**
*   @brief Retrieve the current value of the specified session property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a session property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_ACCELERATION (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, and @c #SCREEN_EVENT_JOYSTICK only)
*   - @c #SCREEN_PROPERTY_BRUSH (for @c #SCREEN_EVENT_MTOUCH_TOUCH and @c #SCREEN_EVENT_POINTER)
*   - @c #SCREEN_PROPERTY_BRUSH_CLIP_POSITION
*   - @c #SCREEN_PROPERTY_BRUSH_CLIP_SIZE
*   - @c #SCREEN_PROPERTY_CURSOR
*   - @c #SCREEN_PROPERTY_DEVICE_COUNT
*   - @c #SCREEN_PROPERTY_FOCUS
*   - @c #SCREEN_PROPERTY_GESTURE_PARAMETERS (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, and @c #SCREEN_EVENT_JOYSTICK only)
*   - @c #SCREEN_PROPERTY_ID [1]
*   - @c #SCREEN_PROPERTY_METRIC_COUNT [1]
*   - @c #SCREEN_PROPERTY_MODE (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_MTOUCH_FOCUS
*   - @c #SCREEN_PROPERTY_OWNER_PID [1]
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_POINTER_FOCUS
*   - @c #SCREEN_PROPERTY_POSITION
*   - @c #SCREEN_PROPERTY_PROTECTION_ENABLE
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR [1]
*   - @c #SCREEN_PROPERTY_SENSITIVITY
*   - @c #SCREEN_PROPERTY_SENSITIVITY_THRESHOLD
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_SIZE
*   - @c #SCREEN_PROPERTY_SPEED (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, and @c #SCREEN_EVENT_JOYSTICK only)
*   - @c #SCREEN_PROPERTY_STATUS
*   - @c #SCREEN_PROPERTY_TRANSFORM [1]
*   - @c #SCREEN_PROPERTY_TYPE [1]
*   - @c #SCREEN_PROPERTY_VISIBLE
*   - @c #SCREEN_PROPERTY_ZORDER
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   @param  ssn The handle of the session whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c int. The buffer may be a single integer or
*                 an array of integers, depending on the property being retrieved.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_session_property_iv(screen_session_t ssn, int pname, int *param);

/**
*   @brief Retrieve the current value of the specified session property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a session property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_IDLE_TIMEOUT (for @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_METRICS
*
*   @param  ssn The handle of the session whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_session_property_llv(screen_session_t ssn, int pname, long long *param);

/**
*   @brief Retrieve the current value of the specified session property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a session property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_ALTERNATE
*   - @c #SCREEN_PROPERTY_BRUSH
*   - @c #SCREEN_PROPERTY_CONTEXT [1]
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_EVENT (for @c #SCREEN_EVENT_JOYSTICK and @c #SCREEN_EVENT_MTOUCH_TOUCH only) [1]
*   - @c #SCREEN_PROPERTY_USER_HANDLE [1]
*   - @c #SCREEN_PROPERTY_WINDOW [2]
*
*   Note [1]: When you're retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   Note [2]: When you're retrieving this property, Screen locally allocates resources to track the
*             window that you retrieve. Therefore, you must call screen_destroy_window() to free
*             these resources when you no longer need to use the window in your local context.
*
*   @param  ssn The handle of the session whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param The buffer where the retrieved value(s) is stored. This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_session_property_pv(screen_session_t ssn, int pname, void **param);

/**
*   @brief Retrieve the current value of the specified session property of type @c float
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function stores the current value of a session property in a user-provided buffer.
*
*   The values of the following properties can be retrieved using this function:
*   - @c #SCREEN_PROPERTY_TRANSFORM [1]
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   @param  ssn The handle of the session whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for query are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c float.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_session_property_fv(screen_session_t ssn, int pname, float *param);

/**
*   @brief Set the value of the specified session property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a session property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*
*   @param  ssn The handle of the session whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The size (that includes a terminating null character) of @c param.
*               Screen reads from @c param until it encounters a null character (`\0`), or until
*               @c len number of bytes is reached. Ensure that @c len doesn't exceed the maximum
*               size that Screen allows for the property specified by @c pname.
*   @param  param A pointer to a character array containing the new property value. This array
*                 must have a minimum length of @c len.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_session_property_cv(screen_session_t ssn, int pname, int len, const char *param);

/**
*   @brief Set the value of the specified session property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a session property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ACCELERATION (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, and @c #SCREEN_EVENT_JOYSTICK only)
*   - @c #SCREEN_PROPERTY_CURSOR
*   - @c #SCREEN_PROPERTY_GESTURE_PARAMETERS (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, and @c #SCREEN_EVENT_JOYSTICK only)
*   - @c #SCREEN_PROPERTY_MODE (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, @c #SCREEN_EVENT_JOYSTICK, and @c #SCREEN_EVENT_MTOUCH_TOUCH only)
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_POSITION
*   - @c #SCREEN_PROPERTY_PROTECTION_ENABLE (application-specific usage, not used by Screen)
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR
*   - @c #SCREEN_PROPERTY_SENSITIVITY
*   - @c #SCREEN_PROPERTY_SENSITIVITY_THRESHOLD
*   - @c #SCREEN_PROPERTY_SIZE
*   - @c #SCREEN_PROPERTY_SPEED (for @c #SCREEN_EVENT_GAMEPAD, @c #SCREEN_EVENT_JOG, and @c #SCREEN_EVENT_JOYSTICK only)
*   - @c #SCREEN_PROPERTY_VISIBLE
*   - @c #SCREEN_PROPERTY_ZORDER
*
*   @param  ssn The handle of the session whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c int. @c param may be a single integer or
*                 an array of integers depending on the property being set.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_session_property_iv(screen_session_t ssn, int pname, const int *param);

/**
*   @brief Set the value of the specified session property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a session property from a user-provided buffer.
*   You can use this function to set the value of the @c #SCREEN_PROPERTY_IDLE_TIMEOUT
*   (for @c #SCREEN_EVENT_MTOUCH_TOUCH only).
*
*   @param  ssn The handle of the session whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_session_property_llv(screen_session_t ssn, int pname, const long long *param);

/**
*   @brief Set the value of the specified session property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a session property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ALTERNATE
*   - @c #SCREEN_PROPERTY_BRUSH
*   - @c #SCREEN_PROPERTY_DISPLAY (only session doesn't have a window)
*   - @c #SCREEN_PROPERTY_USER_HANDLE
*   - @c #SCREEN_PROPERTY_WINDOW
*
*   @param  ssn The handle of the session whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_session_property_pv(screen_session_t ssn, int pname, void **param);

/**
*   @brief Set the value of the specified session property of type @c float
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a session property from a user-provided buffer.
*   Currently, no session properties can be set using this API; it is only made
*   available for consistency with the corresponding 'get' inteface.
*
*   @param  ssn The handle of the session whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c float.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_session_property_fv(screen_session_t ssn, int pname, const float *param);
/** @} */   /* end of ingroup screen_sessions */

/*
 * Streams
 */
/**
*   @ingroup screen_streams
*   @{
*/
/**
*   @brief Associate buffers with a stream
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   You must be working within a @c #SCREEN_BUFFER_PROVIDER_CONTEXT in order to call this function successfully.
*   This function allows a stream to use a buffer that was allocated by the application or Screen.
*   Note that streams can have multiple buffers. For more information, see the <a href="manual/cscreen_buffers_create-attach.xml"> "Buffers" </a>
*   chapter of the <em>Screen Developer's Guide</em>.
*
*   @param  stream The handle of a stream that the buffer is being attached to.
*   @param  count The number of buffers to be attached.
*   @param  buf An array of @c count buffers to be attached to @c stream.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_attach_stream_buffers(screen_stream_t stream, int count, screen_buffer_t *buf);

/**
*   @brief Cause a stream to become a consumer endpoint to another stream
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function is used when the consumer of a stream is in a different
*   process. The function causes the local stream handle to have access to
*   the same buffers as the remote stream. It also connects the local stream
*   as a consumer endpoint of the remote stream.
*
*   @param  stream The handle of the stream that's consuming buffer(s) of a producer.
*   @param  count The number of buffers that the consumer shares with the producer. Use a @c count
*                 of 0 to ensure that the number of buffers shared by the consumer matches
*                 that of the producer.
*   @param  producer The handle of the stream whose buffer(s) is consumed.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_consume_stream_buffers(screen_stream_t stream, int count, screen_stream_t producer);

/**
*   @brief Create a stream that can be used to provide graphical content to generic consumers
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a stream object. The stream size defaults to fullscreen when it's created,
*   if a display is present. Note that after calling @c screen_create_stream(), the object
*   exists locally as a handle. The stream is created in the Screen when buffers are created
*   with @c screen_create_stream_buffers(), when buffers are attached with
*   @c screen_attach_stream_buffers(), or when buffers are shared with
*   @c screen_consume_stream_buffers().
*
*   @param  pstream A pointer to the newly created stream.
*   @param  ctx The connection to Screen. This context must
*               have been created with @c screen_create_context().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_stream(screen_stream_t *pstream, screen_context_t ctx);

/**
*   @brief Send a request to Screen to add new buffers to a stream
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function adds buffers to a stream. Streams must have at least one
*   buffer to be usable. After the producer creates buffers for a stream,
*   or after it attaches buffers to a stream, it must call @c screen_destroy_stream_buffers()
*   before calling @c screen_create_stream_buffers() again.
*   Buffers are created with the size of @c #SCREEN_PROPERTY_BUFFER_SIZE
*   as set on the stream.
*
*   Before calling this function, ensure that you set the @c #SCREEN_PROPERTY_USAGE property on the stream
*   to indicate your intended usage of the stream's buffer(s). For example, to retrieve @c #SCREEN_PROPERTY_POINTER
*   from the buffer(s), you must have set the @c @c #SCREEN_PROPERTY_USAGE property to include the
*   #SCREEN_USAGE_READ and/or @c #SCREEN_USAGE_WRITE flags on the stream prior to calling
*   screen_create_stream_buffers().
*
*   @param  stream The handle of the stream for which the new buffers must be allocated.
*   @param  count The number of buffers to be created for this stream. The maximum number
*                 of buffers allowed is 35.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_stream_buffers(screen_stream_t stream, int count);

/**
*   @brief Destroy a stream and free associated resources
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function destroys the stream associated with the given stream handle.
*   Any resources or buffers created for this stream, both locally and by the
*   windowing system, are released.
*
*   The stream handle can no longer be used as argument to subsequent
*   Screen API calls. Buffers that aren't created by Screen and
*   registered with @c screen_attach_stream_buffers() aren't freed by this
*   operation.
*
*   The buffers aren't freed until all shared users of the buffers have
*   also called @c screen_destroy_stream() or @c screen_destroy_stream_buffers().
*
*   Your application is responsible for releasing its own external
*   buffers. You must use @c screen_destroy_stream() to free streams that were
*   obtained by querying context or event properties because Screen doesn't remove the
*   streams if they still exist. Screen releases only the local state associated with external
*   streams.
*
*   @param  stream The handle of the stream to be destroyed. This must
*                  have been created with @c screen_create_stream().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_stream(screen_stream_t stream);

/**
*   @brief Send a request to Screen to destroy the buffers of the specified stream
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function releases the buffers that were allocated for a stream, without
*   destroying the local object handle. If the buffers were created with
*   @c screen_create_stream_buffers(), the memory is released and can be used for
*   other stream, window or pixmap buffers. If the buffers were attached using
*   @c screen_attach_stream_buffers(), the buffers are destroyed but no memory is
*   actually released. In this case the application is responsible for freeing
*   the memory after calling @c screen_destroy_stream_buffers(). Once the stream
*   buffers have been destroyed, you can change the format, the usage, and the
*   buffer size before creating new buffers again.
*   The memory that is released by this call is not reserved and can be used for
*   any subsequent buffer allocation by Screen.
*   After calling @c screen_destroy_stream_buffers(), the object no longer exists
*   in Screen. Only the local handle remains.
*
*   @param  stream The handle of the stream whose buffers you want to destroy.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_stream_buffers(screen_stream_t stream);

/**
*   @brief Retrieve the current value of the specified stream property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value(s) of a stream property and stores them in a user-provided buffer.
*   The values of the following properties can be queried using this function:
*   - @c #SCREEN_PROPERTY_ID
*   - @c #SCREEN_PROPERTY_ID_STRING
*
*   @param  stream The handle of the stream whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The maximum number of bytes that Screen can write to @c param.
*   @param  param The buffer where Screen stores the retrieved value (a character array that
*                 includes a terminating null character). This buffer must be an array of type
*                 @c char with a minimum length of @c len. If this user-provided buffer has
*                 insufficient space for storing the property being retrieved, Screen
*                 truncates the retrieved value and returns the character array without a
*                 terminating null character.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_stream_property_cv(screen_stream_t stream, int pname, int len, char *param);

/**
*   @brief Retrieve the current value of the specified stream property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value(s) of a stream property and stores them in a user-provided buffer.
*   The values of the following properties can be queried using this function:
*   - @c #SCREEN_PROPERTY_ALPHA_MODE
*   - @c #SCREEN_PROPERTY_BUFFER_COUNT [1]
*   - @c #SCREEN_PROPERTY_BUFFER_SIZE
*   - @c #SCREEN_PROPERTY_COLOR_SPACE
*   - @c #SCREEN_PROPERTY_FORMAT
*   - @c #SCREEN_PROPERTY_FRONT_BUFFER_COUNT [1]
*   - @c #SCREEN_PROPERTY_ID [1]
*   - @c #SCREEN_PROPERTY_METRIC_COUNT [1]
*   - @c #SCREEN_PROPERTY_MODE
*   - @c #SCREEN_PROPERTY_OWNER_PID [1]
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_PROTECTION_ENABLE
*   - @c #SCREEN_PROPERTY_RENDER_BUFFER_COUNT [1]
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR [1]
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_SOURCE_CLIP_POSITION
*   - @c #SCREEN_PROPERTY_SOURCE_CLIP_SIZE
*   - @c #SCREEN_PROPERTY_STATUS
*   - @c #SCREEN_PROPERTY_SWAP_INTERVAL
*   - @c #SCREEN_PROPERTY_USAGE
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   @param  stream The handle of the stream whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*           <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c int. The buffer may be a single integer or
*                 an array of integers, depending on the property being retrieved.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_stream_property_iv(screen_stream_t stream, int pname, int *param);

/**
*   @brief Retrieve the current value of the specified stream property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value(s) of a stream property and stores them in a user-provided buffer.
*   The values of the following properties can be queried using this function:
*    - @c #SCREEN_PROPERTY_METRICS
*
*   @param  stream The handle of the stream whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_stream_property_llv(screen_stream_t stream, int pname, long long *param);

/**
*   @brief Retrieve the current value of the specified stream property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value(s) of a stream property and stores them in a user-provided buffer.
*   The values of the following properties can be queried using this function:
*   - @c #SCREEN_PROPERTY_BUFFERS
*   - @c #SCREEN_PROPERTY_CONTEXT [1]
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_FRONT_BUFFERS [1]
*   - @c #SCREEN_PROPERTY_RENDER_BUFFERS [1]
*   - @c #SCREEN_PROPERTY_USER_HANDLE [1]
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   @param  stream handle of the stream whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_stream_property_pv(screen_stream_t stream, int pname, void **param);

/**
*   @brief Make a stream frame available to its consumers
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Apply Execution</a>
*
*   In addition to the area(s) defined by @c dirty_rects, consumers may use
*   all the pixels in the buffer. Consumers may also retrieve data from the buffer
*   at times other than when @c screen_post_stream() is called.
*
*   It isn't necessary to use multiple threads or application buffer management schemes to render
*    at the full display frame rate because, unlike equivalent calls in
*   other graphics systems, @c screen_post_stream() doesn't always block.
*
*   The function may cause the @c #SCREEN_PROPERTY_RENDER_BUFFERS of the stream
*   to change. Avoid having other threads in the application read this property while one thread
*   is running @c screen_post_stream() on the same stream. If this happens, the
*   property may reflect out-of-date information that can lead to artifacts.
*
*   If @c count is 0, the buffer is discarded and a new set of rendering buffers is returned.
*   The current front buffer remains unchanged.
*
*   @param  stream The handle for the stream whose content has changed.
*   @param  buf The rendering buffer of the stream that needs
*               to be made available to consumers.
*   @param  count The number of rectangles provided in the @c dirty_rects argument, or 0 to indicate
*                 that the entire buffer is dirty.
*   @param  dirty_rects An array of integers containing the x and y coordinates, width, and height
*                       of a rectangle that bounds the area of the rendering buffer that has changed
*                       since the last posting of the window. The @c dirty_rects argument must provide
*                       at least @c count * 4 integers. You can also pass @c NULL if you are
*                       passing 0 for the @c count argument.
*   @param  flags The indication of how you want this function to behave. Valid values for this
*                 parameter are:
*                 - @c #SCREEN_WAIT_IDLE: When you use @c #SCREEN_WAIT_IDLE, this function
*                                         returns only when all consumers have released the previous
*                                         buffer and at least one render buffer is available.
*                 - @c #SCREEN_DONT_BLOCK:
*                         When you use @c #SCREEN_DONT_BLOCK, this function returns immediately
*                         regardless of whether a render buffer is currently available. Do not
*                         use @c SCREEN_PROPERTY_RENDER_BUFFERS after posting with this flag.
*                         Instead, use @c screen_dequeue_stream_render_buffer() to wait for the
*                         next available render buffer.
*                 - @c #SCREEN_EXPLICIT_ENQUEUE:
*                         When you use @c #SCREEN_EXPLICIT_ENQUEUE, this function will leave the
*                         render buffer dequeued. The buffer will remain unavailable to the content
*                         producer until said producer calls @c screen_enqueue_render_buffer() and
*                         all content consumers release the buffer. This will allow the content
*                         producer to continue referencing and reading from the buffer after the
*                         post. Under no circumstances may the content producer continue writing
*                         to the buffer once post has been called.
*                 - 0: When you use 0, this function returns immediately if there is at least one
*                      render buffer available.
*
*   @return @c 0 if the area of the rendering buffer that is marked dirty has
*           updated on the screen and a new set of rendering buffers was
*           returned, or @c -1 if an error occurred (@c errno is set).
*/
int screen_post_stream(screen_stream_t stream, screen_buffer_t buf, int count, const int *dirty_rects, int flags);

/**
*   @brief Create a reference to a stream
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a reference to a stream. This function can
*   be used by libraries to prevent the stream or its buffers from
*   disappearing while the library is using it. The stream and
*   its buffers aren't destroyed until all references have been
*   cleared with @c screen_unref_stream(). In the event that a stream is
*   destroyed before the reference is cleared, @c screen_unref_stream()
*   causes the stream buffer and/or the stream to be destroyed.
*
*   @param  stream The handle of the stream for which the reference
*                  is to be created.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_ref_stream(screen_stream_t stream);

/**
*   @brief Acquire the next available render buffer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function should be used instead of @c #SCREEN_PROPERTY_RENDER_BUFFERS when
*   @c screen_post_stream() is called with the @c #SCREEN_DONT_BLOCK flag.  @c #SCREEN_PROPERTY_RENDER_BUFFERS
*   is not updated when @c screen_post_stream() is called with the @c #SCREEN_DONT_BLOCK
*   flag.  This function takes a buffer from the render buffers.  The buffer
*   will not be available as a render buffer again until one of the following
*   occurs for the buffer:
*
*       1) The producer calls @c screen_post_stream() with the @c #SCREEN_EXPLICIT_ENQUEUE
*          flag, the producer calls @c screen_enqueue_render_buffer() and all consumers call
*          @c screen_release_buffer().
*       2) The producer calls @c screen_post_stream() without the @c #SCREEN_EXPLICIT_ENQUEUE
*          flag and all consumers call screen_release_buffer.
*       3) The producer does NOT call @c screen_post_stream().  Instead, the producer
*          simply calls @c screen_enqueue_render_buffer().
*
*   @param  pbuf A pointer to a location where this function will store a reference to the
*                render buffer it finds.
*   @param  stream The handle of a stream from which a render buffer is to be obtained.
*   @param  flags The indication of how you want this function to behave. This integer is a bitmask
*                 that represents the combination of
*                 <a href="screen_8h_1Screen_Flushing_Types.xml">Screen flushing types</a>:
*                 - @c #SCREEN_DONT_BLOCK: When you use @c #SCREEN_DONT_BLOCK, this
*                                          function returns immediately regardless of whether
*                                          a render buffer is currently available. If there are no
*                                          buffers available, the function returns with
*                                          an @c EWOULDBLOCK error.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_dequeue_stream_render_buffer(screen_buffer_t *pbuf, screen_stream_t stream, int flags);

/**
*   @brief Set the value of the specified stream property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a stream property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*
*   @param  stream The handle of the stream whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The size (that includes a terminating null character) of @c param.
*               Screen reads from @c param until it encounters a null character (`\0`), or until
*               @c len number of bytes is reached. Ensure that @c len doesn't exceed the maximum
*               size that Screen allows for the property specified by @c pname.
*   @param  param A pointer to a character array containing the new property value. This array
*                 must have a minimum length of @c len.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_stream_property_cv(screen_stream_t stream, int pname, int len, const char *param);

/**
*   @brief Set the value of the specified stream property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a stream property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ALPHA_MODE
*   - @c #SCREEN_PROPERTY_BUFFER_SIZE
*   - @c #SCREEN_PROPERTY_COLOR_SPACE
*   - @c #SCREEN_PROPERTY_FORMAT
*   - @c #SCREEN_PROPERTY_MODE
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_PROTECTION_ENABLE
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR
*   - @c #SCREEN_PROPERTY_SOURCE_CLIP_POSITION
*   - @c #SCREEN_PROPERTY_SOURCE_CLIP_SIZE
*   - @c #SCREEN_PROPERTY_SWAP_INTERVAL
*   - @c #SCREEN_PROPERTY_USAGE
*
*   @param  stream The handle of the stream whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c int and @c param may be a single integer or
*                 an array of integers, depending on the property being set.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_stream_property_iv(screen_stream_t stream, int pname, const int *param);

/**
*   @brief Set the value of the specified stream property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a stream property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_TIMESTAMP
*
*   @param  stream The handle of the stream whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c long @c long.
*                 If you pass @c NULL, Screen inserts the system time.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_stream_property_llv(screen_stream_t stream, int pname, const long long *param);

/**
*   @brief Set the value of the specified stream property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a stream property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_USER_HANDLE
*
*   @param  stream The handle of the stream whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_stream_property_pv(screen_stream_t stream, int pname, void **param);

/**
*   @brief Remove a reference from a specified stream
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function removes a reference to a stream. If the stream and its
*   buffers haven't been destroyed yet, the effect of @c screen_unref_stream()
*   is simply to decrease a reference count. If the stream or its buffers
*   were destroyed while still being referenced, @c screen_unref_stream()
*   causes the stream and/or its buffers to be destroyed when the
*   reference count reaches zero.
*
*   @param  stream The handle of the stream for which the reference
*                  is to be removed.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_unref_stream(screen_stream_t stream);
/** @} */   /* end of ingroup screen_streams */

/*
 * Windows
 */
/**
*   @ingroup screen_windows
*   @{
*/
/**
*   @brief Associate buffers with a window
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   You must be working within a @c #SCREEN_BUFFER_PROVIDER_CONTEXT in order to call this function successfully.
*   This function allows a window to use a buffer that was allocated by the application or Screen.
*   Note that windows can have multiple buffers. For more information, see
*   the <a href="manual/cscreen_buffers_create-attach.xml"> "Buffers" </a> chapter of the <em>Screen Developer's Guide</em>.

*
*   @param  win The handle of a window that the buffer is being attached to.
*   @param  count The number of buffers to be attached.
*   @param  buf An array of @c count buffers to be attached to @c win.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_attach_window_buffers(screen_window_t win, int count, screen_buffer_t *buf);

/**
*   @brief Create a window that can be used to make graphical content visible on a display
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a window object. It is equivalent to calling
*   @c screen_create_window_type() with @c type as @c #SCREEN_APPLICATION_WINDOW.
*
*   @param  pwin An address where the function can store the handle to the
*                newly created native window.
*   @param  ctx The connection to Screen. This context must have been created
*               with @c screen_create_context().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_window(screen_window_t *pwin, screen_context_t ctx);

/**
*   @brief Create a new window of a specified type
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a window object of the specified type.  If a class named "default" is
*   defined in the configuration file its properties will be applied to the window at creation time.
*
*   @param  pwin A pointer to the newly created native window.
*   @param  ctx The connection to Screen. This context must have been created
*               with @c screen_create_context().
*   @param  type The type of window to be created. @c type must be of type
*                <a href="group__screen__windows_1Screen_Window_Types.xml">Screen window types</a>.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_window_type(screen_window_t *pwin, screen_context_t ctx, int type);

/**
*   @brief Create a new window of a specified type based on a class
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a window object of the specified type and applies a class's properties to
*   it at creation time.
*
*   This function is associated with the preprocessor definition @c screen_create_window_from_class.
*
*   @param  pwin A pointer to the newly created native window.
*   @param  ctx The connection to Screen. This context must have been created
*               with @c screen_create_context().
*   @param  type The type of window to be created. @c type must be of type
*                <a href="group__screen__windows_1Screen_Window_Types.xml">Screen window types</a>.
*   @param  class_name The name of the class to create the window from.  The class is defined in the
*                      configuration file and its properties will be applied to the window
*                      at creation time.  An empty string will not apply any class.  If @c class is NULL
*                      it is equivalent to calling @c screen_create_window_type().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_window_from_class(screen_window_t *pwin, screen_context_t ctx, int type, const char *class_name);

/**
*   @brief Allocate new buffers for a window
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function allocates buffers for a window. Buffers are created with the size of
*   @c #SCREEN_PROPERTY_BUFFER_SIZE as set for the window.
*
*   Before calling this function, ensure that you set the @c #SCREEN_PROPERTY_USAGE property on the window
*   to indicate your intended usage of the window's buffer(s). For example, to retrieve @c #SCREEN_PROPERTY_POINTER
*   from the buffer(s), you must have set the @c #SCREEN_PROPERTY_USAGE property to include the
*   #SCREEN_USAGE_READ and/or @c #SCREEN_USAGE_WRITE flags on the window prior to calling
*   screen_create_window_buffers().
*
*   @param  win The handle of the window for which the new buffers must be
*               allocated.
*   @param  count The number of buffers to be created for this window. If @c count is 0
*                 the window is considered bufferless and will display the color
*                 specified by @c #SCREEN_PROPERTY_COLOR. The maximum number of buffers
*                 allowed is 35.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_window_buffers(screen_window_t win, int count);

/**
*   @brief Create a window group that other windows can join
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function creates a window group and assigns it to the specified window.
*   The group is identified by the name string, which must be unique. The
*   request fails if another group was previously created with the same
*   name.
*
*   Windows can parent only one group. Therefore, @c screen_create_window_group()
*   can be called successfully only once for any given window.
*   Additionally, only windows of certain types can parent a
*   group of windows. Windows with a type of @c #SCREEN_APPLICATION_WINDOW
*   can parent windows of type @c #SCREEN_CHILD_WINDOW and @c #SCREEN_EMBEDDED_WINDOW.
*   Windows with a type of @c #SCREEN_CHILD_WINDOW can also create a group and parent windows of
*   type @c #SCREEN_EMBEDDED_WINDOW.
*
*   Once a group is created, it exists until the window that parents the group
*   is destroyed. When a parent window is destroyed, all children are orphaned
*   and made invisible. Destroying a child has no effect on the group other than
*   removing the window from the group.
*
*   Group owners have privileged access to the windows that they
*   parent. When windows join the group, the parent receives a
*   @c #SCREEN_EVENT_CREATE event that contains a handle to the child window that can be
*   used by the parent to set properties or send events. Conversely, the parent
*   gets notified when a child window gets destroyed. The parent window is
*   expected to destroy its local copy of the window handle when one of its
*   children is destroyed.
*
*   Call screen_destroy_group() when you no longer need the window group. Note the
*   screen_group_t handle required by @c screen_destroy_group() can be obtained
*   by calling @c screen_get_window_property_pv().
*
*   @param  win The handle of the window for which the group is created. This
*               window must have been created with @c screen_create_window_type() with
*               a type of @c #SCREEN_APPLICATION_WINDOW or @c #SCREEN_CHILD_WINDOW.
*   @param  name A unique string that is used to identify the window group.
*                This name must be communicated to any window that wants to join the
*                group as a child of @c win. Lower case and special characters are permitted in
*                string. However, the string must be unique within your system and must not
*                start with <tt>__scrn-</tt>. If @c name is a NULL pointer, or an empty string,
*                then Screen generates a string for you starting with <tt>__scrn-</tt>.
*                It is recommended that, unless a static name is explicitly required,
*                you should call this function with @c name as NULL (or an empty string) so that a
*                unique group name is automatically generated. You can use
*                @c screen_get_window_property_cv() with @c #SCREEN_PROPERTY_GROUP as
*                the property to retrieve the name of the window group.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_create_window_group(screen_window_t win, const char *name);

/**
*   @brief Destroy a window and free associated resources
*
*   <b>Function Type:</b>
*   - <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a> or
*   - <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   The function type for @c screen_destroy_window() is flushing execution in
*   any of the following cases:
*   - @c win was created with @c screen_create_window()
*   - @c win was created with @c screen_create_window_type()
*   - a reference to @c win was created with @c screen_ref_window()
*
*   Otherwise, this function is of the type immediate execution.
*
*   This function destroys the window associated with the given window handle.
*   If the window is visible, it's removed from the display. Any resources or
*   buffers created for this window, both locally and by Screen, are released.
*
*   The window handle can no longer be used as argument in subsequent
*   Screen API calls.
*
*   Buffers that are not created by Screen and registered with @c screen_attach_window_buffers()
*   are not freed by this function, @c screen_destroy_window(). The application is responsible
*   for releasing its own external buffers. Any window that shares buffers with
*   the window is also destroyed.
*
*   @c screen_destroy_window() must be used to free windows that were
*   obtained by querying context or event properties. In this case, the
*   window isn't removed from its display and destroyed. Only the local state
*   associated with the external window is released.
*
*   @param  win The handle of the window to be destroyed. This must
*               have been created with @c screen_create_window().
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_window(screen_window_t win);

/**
*   @brief Destroy buffers of the specified window
*
*   <b>Function Type:</b>
*   - <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a> or
*   - <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   If @c win was created with @c screen_create_window_buffers() or a reference
*   to @c win was created with @c screen_ref_window(), then the function type
*   for @c screen_destroy_window_buffers() is flushing execution. Otherwise, the
*   function type is immediate execution.
*
*   This function releases one or more buffers allocated for the specified
*   window, without destroying the window. If buffers were created with
*   @c screen_create_window_buffers(), the memory is released and can be used for
*   other window or pixmap buffers. If buffers were attached using
*   @c screen_attach_window_buffers(), these buffers are destroyed but no memory
*   is actually released. In this case, the application is responsible for freeing
*   the memory after calling @c screen_destroy_window_buffers(). The memory that
*   is released by this call is not reserved and can be used for any subsequent
*   buffer allocation by Screen.
*
*   @param  win The handle of the window whose buffer(s) you want to destroy.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_destroy_window_buffers(screen_window_t win);

/**
*   @brief Discard the specified window regions
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function is a hole-punching API. Use this function to specify window
*   regions you want to discard. The regions behave as if they were transparent,
*   or as if there were no transparency on the window. When you call this
*   function, it invalidates any regions you might have defined previously. You
*   can call this function with @c count set to 0 to remove discarded regions.
*
*   @param  win The handle of the window in which you want to specify regions
*               to discard.
*   @param  count The number of rectangles (rectangular regions) you want to
*                 discard, specified in the @c rects argument. The value of @c count
*                 can be @c 0.
*   @param  rects An array of integers containing the x, y, width, and height
*                 coordinates of rectangles that bound areas in the window you want to
*                 discard. The @c rects argument must provide at least 4 times @c count
*                 integers(quadruples of x, y, width and height).
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_discard_window_regions(screen_window_t win, int count, const int *rects);

/**
*   @brief Retrieve the current value of the specified window property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value(s) of a window property and stores them in a user-provided buffer.
*   The values of the following properties can be queried using this function:
*   - @c #SCREEN_PROPERTY_CLASS
*   - @c #SCREEN_PROPERTY_ID
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_GROUP
*   - @c #SCREEN_PROPERTY_PARENT
*
*   @param  win The handle of the window whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The maximum number of bytes that Screen can write to @c param.
*   @param  param The buffer where Screen stores the retrieved value (a character array that
*                 includes a terminating null character). This buffer must be an array of type
*                 @c char with a minimum length of @c len. If this user-provided buffer has
*                 insufficient space for storing the property being retrieved, Screen
*                 truncates the retrieved value and returns the character array without a
*                 terminating null character.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_window_property_cv(screen_window_t win, int pname, int len, char *param);

/**
*   @brief Retrieve the current value of the specified window property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value(s) of a window property and stores them in a user-provided buffer.
*   The values of the following properties can be queried using this function:
*   - @c #SCREEN_PROPERTY_ALPHA_MODE
*   - @c #SCREEN_PROPERTY_BRIGHTNESS
*   - @c #SCREEN_PROPERTY_BUFFER_COUNT [1]
*   - @c #SCREEN_PROPERTY_BUFFER_SIZE
*   - @c #SCREEN_PROPERTY_CBABC_MODE
*   - @c #SCREEN_PROPERTY_CLIP_POSITION
*   - @c #SCREEN_PROPERTY_CLIP_SIZE
*   - @c #SCREEN_PROPERTY_COLOR
*   - @c #SCREEN_PROPERTY_COLOR_SPACE
*   - @c #SCREEN_PROPERTY_CONTRAST
*   - @c #SCREEN_PROPERTY_DEBUG
*   - @c #SCREEN_PROPERTY_FLIP
*   - @c #SCREEN_PROPERTY_FLOATING
*   - @c #SCREEN_PROPERTY_FOCUS
*   - @c #SCREEN_PROPERTY_FORMAT
*   - @c #SCREEN_PROPERTY_GLOBAL_ALPHA
*   - @c #SCREEN_PROPERTY_HUE
*   - @c #SCREEN_PROPERTY_ID [1]
*   - @c #SCREEN_PROPERTY_IDLE_MODE
*   - @c #SCREEN_PROPERTY_METRIC_COUNT [1]
*   - @c #SCREEN_PROPERTY_MIRROR
*   - @c #SCREEN_PROPERTY_OWNER_PID [1]
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_PIPELINE
*   - @c #SCREEN_PROPERTY_POSITION
*   - @c #SCREEN_PROPERTY_PROTECTION_ENABLE
*   - @c #SCREEN_PROPERTY_RENDER_BUFFER_COUNT [1]
*   - @c #SCREEN_PROPERTY_ROTATION
*   - @c #SCREEN_PROPERTY_SATURATION
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR [1]
*   - @c #SCREEN_PROPERTY_SCALE_QUALITY
*   - @c #SCREEN_PROPERTY_SELF_LAYOUT
*   - @c #SCREEN_PROPERTY_SENSITIVITY
*   - @c #SCREEN_PROPERTY_SENSITIVITY_THRESHOLD
*   - @c #SCREEN_PROPERTY_SEQUENCE_ID
*   - @c #SCREEN_PROPERTY_SIZE
*   - @c #SCREEN_PROPERTY_SOURCE_CLIP_POSITION
*   - @c #SCREEN_PROPERTY_SOURCE_CLIP_SIZE
*   - @c #SCREEN_PROPERTY_SOURCE_POSITION
*   - @c #SCREEN_PROPERTY_SOURCE_SIZE
*   - @c #SCREEN_PROPERTY_STATIC
*   - @c #SCREEN_PROPERTY_STATUS
*   - @c #SCREEN_PROPERTY_SWAP_INTERVAL
*   - @c #SCREEN_PROPERTY_TRANSFORM
*   - @c #SCREEN_PROPERTY_TRANSPARENCY
*   - @c #SCREEN_PROPERTY_TYPE
*   - @c #SCREEN_PROPERTY_USAGE
*   - @c #SCREEN_PROPERTY_VIEWPORT_POSITION
*   - @c #SCREEN_PROPERTY_VIEWPORT_SIZE
*   - @c #SCREEN_PROPERTY_VISIBLE
*   - @c #SCREEN_PROPERTY_ZORDER
*
*   Note [1]: When retrieving this property, the function type is
*             <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   @param  win The handle of the window whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c int. The buffer may be a single integer or
*                 an array of integers, depending on the property being retrieved.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_window_property_iv(screen_window_t win, int pname, int *param);

/**
*   @brief Retrieve the current value of the specified window property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value(s) of a window property and stores them in a user-provided buffer.
*   The values of the following properties can be queried using this function:
*    - @c #SCREEN_PROPERTY_METRICS
*
*   @param  win handle of the window whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_window_property_llv(screen_window_t win, int pname, long long *param);

/**
*   @brief Retrieve the current value of the specified window property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value(s) of a window property and stores them in a user-provided buffer.
*   The values of the following properties can be queried using this function:
*   - @c #SCREEN_PROPERTY_BUFFERS
*   - @c #SCREEN_PROPERTY_ALTERNATE
*   - @c #SCREEN_PROPERTY_BRUSH
*   - @c #SCREEN_PROPERTY_CONTEXT [1]
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_FRONT_BUFFER [1]
*   - @c #SCREEN_PROPERTY_GROUP [2]
*   - @c #SCREEN_PROPERTY_PARENT [2]
*   - @c #SCREEN_PROPERTY_RENDER_BUFFERS [1]
*   - @c #SCREEN_PROPERTY_STREAM [1],[2]
*   - @c #SCREEN_PROPERTY_USER_HANDLE [1]
*
*   Note [1]: When you're retrieving this property, the function type is
*           <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>.
*
*   Note [2]: When you're retrieving this property, Screen locally allocates resources to track the
*             object that you retrieve. Therefore, you must call the appropriate
*             screen_destroy_*() function to free these resources when you no longer need to use the object
*             in your local context.
*
*   @param  win handle of the window whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_window_property_pv(screen_window_t win, int pname, void **param);

/**
*   @brief Retrieve the current value of the specified window property of type @c float
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function retrieves the value(s) of a window property and stores them in a user-provided buffer.
*   The values of the following properties can be queried using this function:
*   - @c #SCREEN_PROPERTY_BRIGHTNESS
*   - @c #SCREEN_PROPERTY_CONTRAST
*   - @c #SCREEN_PROPERTY_HUE
*   - @c #SCREEN_PROPERTY_SATURATION
*   - @c #SCREEN_PROPERTY_TRANSFORM
*
*   @param  win The handle of the window whose property is being queried.
*   @param  pname The name of the property whose value is being queried. The
*                 properties available for querying are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer where this function stores the retrieved value(s). This
*                 buffer must be of type @c float.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_get_window_property_fv(screen_window_t win, int pname, float *param);

/**
*   @brief Cause a window to join a group or a window group
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function is used to add a window to a group or window group.
*
*   <b>Window group:</b>
*   A window group is a hierarchy of windows. When an application joins a window group, it becomes
*   parented (managed) by the window whose group it joined. Child and embedded windows remain
*   invisible until they're parented.
*
*   Until the window joins a window group, a window of any type behaves like an
*   application window. The window's positioning and visibility are not relative
*   to any other window on the display.
*
*   Once a window successfully joins a window group, its position on the display is
*   relative to the parent. The type of the window determines exactly how the
*   window is positioned. Child windows are positioned relative to their
*   parent (i.e., their window position is added to the parent's window position.
*   Embedded windows are positioned relative to the source viewport of the
*   parent.
*
*   Windows in a window group inherit the visibility and the global transparency
*   of their parent.
*
*   <b>Group:</b>
*   A group of windows is a collection of windows that share properties that can be set through
*   @c screen_set_group_property_*(). Unlike a window group, there's no implication of
*   hierarchy for windows in a group.
*
*   @param  win The handle for the window that is to join the group.
*   @param  name A unique string that identifies the group. This is the #SCREEN_PROPERTY_NAME
*                property of the group that the specified window is joining.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_join_window_group(screen_window_t win, const char *name);

/**
*   @brief Cause a window to leave a window group
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function removes a window from a window group.
*
*   @param  win The handle for the window that is to leave the group.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_leave_window_group(screen_window_t win);

/**
*   @brief Temporarily transfer the ownership of a window to a manager or parent
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Flushing Execution</a>
*
*   This function notifies one or more managers that this window wants to be
*   managed. At this point, the ownership of the window is transferred to the
*   manager, or parent. The manager, or parent, can configure some of the
*   window's properties (those that could previously only be changed by the
*   owner) so that the manager's requirements are satisfied before allowing
*   execution to continue. For example, the window manager may set the buffer's
*   size (@c #SCREEN_PROPERTY_BUFFER_SIZE).
*
*   When the manager has completed managing the window, it sets the property,
*   @c #SCREEN_PROPERTY_MANAGER_STRING to unblock this function. The original
*   owner receives a @c #SCREEN_EVENT_PROPERTY event when this property is set.
*   At this point, the ownership of the window is transferred from the manager,
*   or parent, back to the original window owner. Typically, this function is
*   called immediately after the window is created.
*
*   Note:
*   - An application can call screen_manage_window() only once per window to be managed.
*   - This function blocks the calling thread and returns when the manager is completed
*     managing the window.
*
*   When a window manager, or a parent window, doesn't exist for @c win, then
*   an error is returned and @c errno is set to @c EAGAIN. If this is the case,
*   your window owner can manage all of its properties.
*
*   @param  win  The window to be managed.
*   @param  data The string to be passed to the window manager. Refer to
*                your associated manager for an appropriate value for this
*                string. @c data must not be NULL.
*
*   @return @c 0 if successful; @c -1 if an error occurred, or
*           no manager or parent is available (@c errno is set; refer to
*           @c errno.h for more details).
*/
int screen_manage_window(screen_window_t win, const char *data);

/**
*   @brief Make window content updates visible on the display
*
*   <b>Function Type:</b>  <a href="manual/rscreen_apply_execution.xml">Apply Execution</a>
*
*   This function makes some pixels in a rendering buffer visible. The pixels to
*   be posted are defined by the dirty rectangles contained in the
*   @c dirty_rects argument. Note that a window will not be made visible until
*   @c screen_post_window() has been called at least once.
*
*   In addition to the area(s) defined by @c dirty_rects, Screen may update
*   the other pixels in the buffer (i.e., Screen posts entire buffers, using
*   @c dirty_rects as a guide). Therefore, your application must ensure that the entire
*   contents of a window buffer is suitable for display at all times.
*
*   The use of multiple threads or application buffer management schemes to render at the full
*   display frame rate aren't necessary because unlike equivalent calls in
*   other graphics systems, @c screen_post_window() doesn't always block.
*
*   This function may cause the @c #SCREEN_PROPERTY_RENDER_BUFFERS property of
*   the posting window to change. At any time, only one thread must operate on,
*   or render, to this window. If your application uses multiple threads, you
*   must ensure that access to this window's handle by these threads is guarded.
*   If not, @c #SCREEN_PROPERTY_RENDER_BUFFERS may reflect out-of-date information
*   that can lead to animation artifacts. The presentation of new content may
*   result in a copy or a buffer flip, depending on how the composited windowing
*   system chooses to perform the operation. Use the window property
*   @c #SCREEN_PROPERTY_RENDER_BUFFER_COUNT to determine the number of buffers
*   you have that are available for rendering.
*
*   @param  win The handle for the window whose content has changed.
*   @param  buf The rendering buffer of the window that contains the changes
*               needed to be made visible. Most applications use two buffers
*               for rendering. Therefore, the simplest way is to use the buffer
*               in the first element of the @c #SCREEN_PROPERTY_RENDER_BUFFERS
*               property of the window. Screen rotates the buffer handles accordingly
*               so that the first buffer handle of @c #SCREEN_PROPERTY_RENDER_BUFFERS
*               is one that is available.
*   @param  count The number of rectangles provided in the @c dirty_rects argument, or 0 to indicate
*                 that the entire buffer is dirty.
*   @param  dirty_rects An array of integers containing the x and y coordinates, width, and height
*                       of a rectangle that bounds the area of the rendering buffer that has changed
*                       since the last posting of the window. The @c dirty_rects argument must provide
*                       at least @c count * 4 integers. You can also pass @c NULL if you are
*                       passing 0 for the @c count argument.
*   @param  flags The indication of how you want this function to behave. Valid values for this
*                 parameter are:
*                 - @c #SCREEN_WAIT_IDLE: When you use @c #SCREEN_WAIT_IDLE, this function
*                                         returns only when the display is updated with the buffer
*                                         from the @c screen_post_window() call and at least one
*                                         render buffer is available.
*                 - @c #SCREEN_DONT_BLOCK:
*                         When you use @c #SCREEN_DONT_BLOCK, this function returns immediately
*                         regardless of whether a render buffer is currently available. Do not
*                         use @c SCREEN_PROPERTY_RENDER_BUFFERS after posting with this flag.
*                         Instead, use @c screen_dequeue_window_render_buffer() to wait for the
*                         next available render buffer.
*                 - @c #SCREEN_EXPLICIT_ENQUEUE:
*                         When you use @c #SCREEN_EXPLICIT_ENQUEUE, this function will leave the
*                         render buffer dequeued. The buffer will remain unavailable to the content
*                         producer until said producer calls @c screen_enqueue_render_buffer() and
*                         all content consumers release the buffer. This will allow the content
*                         producer to continue referencing and reading from the buffer after the
*                         post. Under no circumstances may the content producer continue writing
*                         to the buffer once post has been called.
*                 - 0: When you use 0, this function returns immediately if there is at least one
*                      render buffer available while respecting the swap interval of the window.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_post_window(screen_window_t win, screen_buffer_t buf, int count, const int *dirty_rects, int flags);

/**
*   @brief Take a screenshot of the window and stores the resulting image in
*          the specified buffer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_apply_execution.xml">Apply Execution</a>
*
*   This function takes the visible content of a window and stores the result in a
*   user-provided buffer. The buffer can be a pixmap buffer or a window buffer.
*   The buffer must have been created with the usage flag @c #SCREEN_USAGE_NATIVE
*   in order for the operation to succeed. This call blocks until the operation
*   is completed. If @c count is 0 and @c save_rects is NULL, the entire window
*   is grabbed. Otherwise, @c save_rects must point to @c count * 4 integers
*   defining rectangles in screen coordinates that need to be grabbed. Note that
*   the buffer size does not have to match the window size. Scaling is
*   applied to make the screenshot fit into the buffer provided.
*
*   All visible content, from @c win and its child windows (if any), is copied to @c buf. You can
*   access the visible content of @c win without having to share its buffer, and buffers of its
*   children. For more information on sharing buffers when there's a window hierarchy, refer to
*   the <em>Screen Developer's Guide</em>.
*
*   @param  win The handle of the window that is the target of the screenshot.
*   @param  buf The buffer where the pixel data is copied to.
*   @param  count The number of rectangles supplied in the @c save_rects
*                 argument.
*   @param  save_rects A pointer to (@c count * 4) integers that define the
*                      areas of the window that need to be grabbed for the screenshot.
*   @param  flags For future use. Pass 0 for now.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_read_window(screen_window_t win, screen_buffer_t buf, int count, const int *save_rects, int flags);

/**
*   @brief Create a reference to a window
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function creates a reference to a window. This function can be used by
*   window managers and group parents to prevent a window from disappearing,
*   even when the process that originally created the window terminates
*   abnormally. If this happens, ownership of the window is transferred to the
*   window manager or group parent. The restrictions imposed on buffers still
*   exist. The contents of the buffers can't be changed. The buffers can't be
*   destroyed until the window is unreferenced. When the original process owner
*   is no longer a client of Screen, the window is destroyed
*   when @c screen_destroy_window() is called by the reference owner.
*
*   @param  win The handle of the window for which the reference is to be
*               created.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_ref_window(screen_window_t win);

/**
*   @brief Acquire the next available render buffer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function should be used instead of @c #SCREEN_PROPERTY_RENDER_BUFFERS when
*   @c screen_post_window() is called with the @c #SCREEN_DONT_BLOCK flag.  @c #SCREEN_PROPERTY_RENDER_BUFFERS
*   is not updated when @c screen_post_window() is called with the @c #SCREEN_DONT_BLOCK
*   flag.  This function takes a buffer from the render buffers.  The buffer
*   will not be available as a render buffer again until one of the following
*   occurs for the buffer:
*
*       1) The producer calls @c screen_post_window() with the @c #SCREEN_EXPLICIT_ENQUEUE
*          flag, the producer calls @c screen_enqueue_render_buffer() and all consumers call
*          @c screen_release_buffer().
*       2) The producer calls @c screen_post_window() without the @c #SCREEN_EXPLICIT_ENQUEUE
*          flag and all consumers call screen_release_buffer.
*       3) The producer does NOT call @c screen_post_window().  Instead, the producer
*          simply calls @c screen_enqueue_render_buffer().
*
*   @param  pbuf A pointer to a location where this function will store a reference to the
*                render buffer it finds.
*   @param  win The handle of a window from which a render buffer is to be obtained.
*   @param  flags The indication of how you want this function to behave. This integer is a bitmask
*                 that represents the combination of
*                 <a href="screen_8h_1Screen_Flushing_Types.xml">Screen flushing types</a>:
*                 - @c #SCREEN_DONT_BLOCK: When you use @c #SCREEN_DONT_BLOCK, this
*                                          function returns immediately regardless of whether
*                                          a render buffer is currently available. If there are no
*                                          buffers available, the function returns with
*                                          an @c EWOULDBLOCK error.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_dequeue_window_render_buffer(screen_buffer_t *pbuf, screen_window_t win, int flags);

/**
*   @brief Set the value of the specified window property of type char
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a window property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_CLASS
*   - @c #SCREEN_PROPERTY_ID_STRING
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*
*   @param  win handle of the window whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  len The size (that includes a terminating null character) of @c param.
*               Screen reads from @c param until it encounters a null character (`\0`), or until
*               @c len number of bytes is reached. Ensure that @c len doesn't exceed the maximum
*               size that Screen allows for the property specified by @c pname.
*   @param  param A pointer to a character array containing the new property value. This array
*                 must have a minimum length of @c len.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_window_property_cv(screen_window_t win, int pname, int len, const char *param);

/**
*   @brief Set the value of the specified window property of type integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a window property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ALPHA_MODE
*   - @c #SCREEN_PROPERTY_BRIGHTNESS
*   - @c #SCREEN_PROPERTY_BUFFER_SIZE
*   - @c #SCREEN_PROPERTY_CBABC_MODE
*   - @c #SCREEN_PROPERTY_CLIP_POSITION
*   - @c #SCREEN_PROPERTY_CLIP_SIZE
*   - @c #SCREEN_PROPERTY_COLOR
*   - @c #SCREEN_PROPERTY_COLOR_SPACE
*   - @c #SCREEN_PROPERTY_CONTRAST
*   - @c #SCREEN_PROPERTY_DEBUG
*   - @c #SCREEN_PROPERTY_FLIP
*   - @c #SCREEN_PROPERTY_FLOATING
*   - @c #SCREEN_PROPERTY_FORMAT
*   - @c #SCREEN_PROPERTY_GLOBAL_ALPHA
*   - @c #SCREEN_PROPERTY_HUE
*   - @c #SCREEN_PROPERTY_IDLE_MODE
*   - @c #SCREEN_PROPERTY_MIRROR
*   - @c #SCREEN_PROPERTY_PERMISSIONS
*   - @c #SCREEN_PROPERTY_PIPELINE
*   - @c #SCREEN_PROPERTY_POSITION
*   - @c #SCREEN_PROPERTY_PROTECTION_ENABLE
*   - @c #SCREEN_PROPERTY_ROTATION
*   - @c #SCREEN_PROPERTY_SATURATION
*   - @c #SCREEN_PROPERTY_SCALE_FACTOR
*   - @c #SCREEN_PROPERTY_SCALE_QUALITY
*   - @c #SCREEN_PROPERTY_SELF_LAYOUT
*   - @c #SCREEN_PROPERTY_SENSITIVITY
*   - @c #SCREEN_PROPERTY_SENSITIVITY_THRESHOLD
*   - @c #SCREEN_PROPERTY_SIZE
*   - @c #SCREEN_PROPERTY_SOURCE_CLIP_POSITION
*   - @c #SCREEN_PROPERTY_SOURCE_CLIP_SIZE
*   - @c #SCREEN_PROPERTY_SOURCE_POSITION
*   - @c #SCREEN_PROPERTY_SOURCE_SIZE
*   - @c #SCREEN_PROPERTY_STATIC
*   - @c #SCREEN_PROPERTY_STATUS
*   - @c #SCREEN_PROPERTY_SWAP_INTERVAL
*   - @c #SCREEN_PROPERTY_TRANSPARENCY
*   - @c #SCREEN_PROPERTY_USAGE
*   - @c #SCREEN_PROPERTY_VIEWPORT_POSITION
*   - @c #SCREEN_PROPERTY_VIEWPORT_SIZE
*   - @c #SCREEN_PROPERTY_VISIBLE
*   - @c #SCREEN_PROPERTY_ZORDER
*
*   @param  win handle of the window whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c int. @c param may be a single integer or
*                 an array of integers depending on the property being set.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_window_property_iv(screen_window_t win, int pname, const int *param);

/**
*   @brief Set the value of the specified window property of type @c long @c long integer
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a window property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_TIMESTAMP
*
*   @param  win handle of the window whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c long @c long.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_window_property_llv(screen_window_t win, int pname, const long long *param);

/**
*   @brief Set the value of the specified window property of type void*
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a window property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_ALTERNATE
*   - @c #SCREEN_PROPERTY_BRUSH
*   - @c #SCREEN_PROPERTY_DISPLAY
*   - @c #SCREEN_PROPERTY_USER_HANDLE
*
*   @param  win handle of the window whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*           properties that you can set are of type
*           <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*           buffer must be of type @c void*.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_window_property_pv(screen_window_t win, int pname, void **param);

/**
*   @brief Set the value of the specified window property of type @c float
*
*   <b>Function Type:</b>  <a href="manual/rscreen_delayed_execution.xml">Delayed Execution</a>
*
*   This function sets the value of a window property from a user-provided buffer.
*   You can use this function to set the value of the following properties:
*   - @c #SCREEN_PROPERTY_BRIGHTNESS
*   - @c #SCREEN_PROPERTY_CONTRAST
*   - @c #SCREEN_PROPERTY_HUE
*   - @c #SCREEN_PROPERTY_SATURATION
*
*   @param  win handle of the window whose property is being set.
*   @param  pname The name of the property whose value is being set. The
*                 properties that you can set are of type
*                 <a href="screen_8h_1Screen_Property_Types.xml">Screen property types</a>.
*   @param  param A pointer to a buffer containing the new value(s). This
*                 buffer must be of type @c float.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_set_window_property_fv(screen_window_t win, int pname, const float *param);

/**
*   @brief Cause a window to display content visible on another display
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   The specified display shares its buffers with the specified window @c win. This method of
*   buffer sharing results in the window's buffers having the same content as the display.
*   For more information, see the "Displays" section of
*   the <a href="manual/cscreen_resource_sharing.xml">Resource Sharing</a> chapter of the
*   <em>Screen Developer's Guide</em>.
*
*   To share display buffers, you need to be working within a privileged
*   context. Therefore, a context that was created with the type
*   @c #SCREEN_DISPLAY_MANAGER_CONTEXT must be used.
*
*   If the display has a framebuffer, then @c screen_share_display_buffers() is similar
*   to @c screen_share_window_buffers().
*
*   @param  win The handle of the window that the display is sharing its buffer(s) with.
*   @param  share The handle of the display who is sharing its buffer(s) with the specified window.
*   @param  count The number of buffers that is shared by the display and the window.
*                 Use a @c count of 0 to ensure that the number of buffers shared by the window
*                 matches that of the display.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_share_display_buffers(screen_window_t win, screen_display_t share, int count);

/**
*   @brief Cause a window to display content produced by another stream
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function is used when a window needs to share the content of another
*   stream. For this operation to be successful, the stream that's the owner of
*   the buffer(s) to be shared must have at least one buffer that was created
*   with @c screen_create_stream_buffers(), or attached with
*   @c screen_attach_stream_buffers(). Updates can be posted using only the stream
*   that's the owner of the buffers (i.e., the stream whose handle is identified as
*   @c share).
*
*   The following properties of the window are ignored if you set them prior to calling this function:
*   - @c #SCREEN_PROPERTY_FORMAT
*   - @c #SCREEN_PROPERTY_BUFFER_SIZE
*
*   @param  win The handle of the window that is sharing the buffer(s) owned
*           by another stream.
*   @param  share The handle of the stream whose buffer(s) is or are to be displayed.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_share_stream_buffers(screen_window_t win, screen_stream_t share);

/**
*   @brief Cause a window to display content produced in another window
*
*   <b>Function Type:</b>  <a href="manual/rscreen_flushing_execution.xml">Flushing Execution</a>
*
*   This function is used when a window needs to share the content of another
*   window. For this operation to be successful, the window that is the owner of
*   the buffer(s) to be shared must have at least one buffer that was created
*   with @c screen_create_window_buffers(), or attached with
*   @c screen_attach_window_buffer(). Updates can only be posted using the window
*   that is the owner of the buffers (i.e. the window whose handle is identified as
*   @c share). You can use the @c screen_share_window_buffers() function to improve
*   performance by reducing the amount of blending on the screen. For example, a
*   window might be entirely transparent except for a watermark that needs to be
*   blended in a corner. Blending the entire window is costly and can be avoided by
*   setting the transparency of this window to @c #SCREEN_TRANSPARENCY_DISCARD. To
*   keep the watermark visible, another window can be created and made to share
*   buffers with the main window. This way, most of the window is discarded and a
*   much smaller area is actually blended.
*
*   The following properties of the window are ignored if you set them prior to calling this function:
*   - @c #SCREEN_PROPERTY_FORMAT
*   - @c #SCREEN_PROPERTY_BUFFER_SIZE
*
*   @param  win The handle of the window that's sharing the buffer(s) owned
*           by another window.
*   @param  share The handle of the window whose buffer(s) is to be displayed.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_share_window_buffers(screen_window_t win, screen_window_t share);

/**
*   @brief Remove a reference from a specified window
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This function removes a reference to a window. When a window is being referenced,
*   its buffers cannot be destroyed until all references to that window have been
*   removed.
*
*   @param  win The handle of the window for which the reference is to be
*           removed.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_unref_window(screen_window_t win);

/**
*   @brief Add a wait for a post on a window
*
*   <b>Function Type:</b>  <a href="manual/rscreen_immediate_execution.xml">Immediate Execution</a>
*
*   This call blocks until there is a post event for the window you are waiting on.
*   This function is typically used in conjunction with @c screen_share_display_buffers()
*   and/or @c screen_share_window_buffers().
*
*   @param  win The handle for the window whose post you are waiting on.
*   @param  flags The indication of how you want this function to behave. Valid values for this
*                 parameter are:
*                 - @c #SCREEN_DONT_BLOCK: When you use @c #SCREEN_DONT_BLOCK, this function
*                                         returns immediately.
*                 - 0: When you use 0, this function blocks until there's a @c #SCREEN_EVENT_POST
*                      event from @c win.
*
*   @return @c 0 if successful, or @c -1 if an error occurred
*           (@c errno is set; refer to @c errno.h for more details).
*/
int screen_wait_post(screen_window_t win, int flags);
/** @} */   /* end of ingroup screen_windows */

#include <_packpop.h>

__END_DECLS

#endif /* _SCREEN_SCREEN_H_ */

#if defined(__QNXNTO__) && defined(__USESRCVERSION)
#include <sys/srcversion.h>
__SRCVERSION("$URL: http://svn.ott.qnx.com/product/graphics/branches/release-4.0.2/lib/screen/public/screen/screen.h $ $Rev: 1021759 $")
#endif
