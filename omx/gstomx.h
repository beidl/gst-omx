/*
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 * Copyright (C) 2013, Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __GST_OMX_H__
#define __GST_OMX_H__

#include <gst/gst.h>
#include <string.h>
#include <OMX_Core.h>
#include <OMX_Component.h>

#include <gst/gstgralloc.h>
#include <gst/gstnativebuffer.h>

G_BEGIN_DECLS

#define GST_OMX_INIT_STRUCT(st) G_STMT_START { \
  memset ((st), 0, sizeof (*(st))); \
  (st)->nSize = sizeof (*(st)); \
  (st)->nVersion.s.nVersionMajor = 1; \
  (st)->nVersion.s.nVersionMinor = 1; \
} G_STMT_END

/* Different hacks that are required to work around
 * bugs in different OpenMAX implementations
 */
/* In the EventSettingsChanged callback use nData2 instead of nData1 for
 * the port index. Happens with Bellagio.
 */
#define GST_OMX_HACK_EVENT_PORT_SETTINGS_CHANGED_NDATA_PARAMETER_SWAP G_GUINT64_CONSTANT (0x0000000000000001)
/* In the EventSettingsChanged callback assume that port index 0 really
 * means port index 1. Happens with the Bellagio ffmpegdist video decoder.
 */
#define GST_OMX_HACK_EVENT_PORT_SETTINGS_CHANGED_PORT_0_TO_1          G_GUINT64_CONSTANT (0x0000000000000002)
/* If the video framerate is not specified as fraction (Q.16) but as
 * integer number. Happens with the Bellagio ffmpegdist video encoder.
 */
#define GST_OMX_HACK_VIDEO_FRAMERATE_INTEGER                          G_GUINT64_CONSTANT (0x0000000000000004)
/* If the SYNCFRAME flag on encoder output buffers is not used and we
 * have to assume that all frames are sync frames.
 * Happens with the Bellagio ffmpegdist video encoder.
 */
#define GST_OMX_HACK_SYNCFRAME_FLAG_NOT_USED                          G_GUINT64_CONSTANT (0x0000000000000008)
/* If the component needs to be re-created if the caps change.
 * Happens with Qualcomm's OpenMAX implementation.
 */
#define GST_OMX_HACK_NO_COMPONENT_RECONFIGURE                         G_GUINT64_CONSTANT (0x0000000000000010)

/* If the component does not accept empty EOS buffers.
 * Happens with Qualcomm's OpenMAX implementation.
 */
#define GST_OMX_HACK_NO_EMPTY_EOS_BUFFER                              G_GUINT64_CONSTANT (0x0000000000000020)

/* If the component might not acknowledge a drain.
 * Happens with TI's Ducati OpenMAX implementation.
 */
#define GST_OMX_HACK_DRAIN_MAY_NOT_RETURN                             G_GUINT64_CONSTANT (0x0000000000000040)

/* If the component doesn't allow any component role to be set.
 * Happens with Broadcom's OpenMAX implementation.
 */
#define GST_OMX_HACK_NO_COMPONENT_ROLE                                G_GUINT64_CONSTANT (0x0000000000000080)

/* If the OpenMAX core should be loaded via libhybris or not
 * This is used to load Android binaries
 */
#define GST_OMX_HACK_HYBRIS                                           G_GUINT64_CONSTANT (0x0000000000000100)

/* If we should try to use Android native buffers for buffer allocation
 * This can be used only with decoders
 */
#define GST_OMX_HACK_ANDROID_BUFFERS                                  G_GUINT64_CONSTANT (0x0000000000000200)

/* The component can detect format changes in the bit stream and will
 * update the output port settings without the input being reconfigured.
 */
#define GST_OMX_HACK_IMPLICIT_FORMAT_CHANGE                           G_GUINT64_CONSTANT (0x0000000000000400)

typedef struct _GstOMXCore GstOMXCore;
typedef struct _GstOMXPort GstOMXPort;
typedef enum _GstOMXPortDirection GstOMXPortDirection;
typedef struct _GstOMXComponent GstOMXComponent;
typedef struct _GstOMXBuffer GstOMXBuffer;
typedef struct _GstOMXMessage GstOMXMessage;

typedef enum {
  /* Everything good and the buffer is valid */
  GST_OMX_ACQUIRE_BUFFER_OK = 0,
  /* The port is flushing, exit ASAP */
  GST_OMX_ACQUIRE_BUFFER_FLUSHING,
  /* The port must be reconfigured */
  GST_OMX_ACQUIRE_BUFFER_RECONFIGURE,
  /* The port was reconfigured and the caps might have changed
   * NOTE: This is only returned a single time! */
  GST_OMX_ACQUIRE_BUFFER_RECONFIGURED,
  /* A fatal error happened */
  GST_OMX_ACQUIRE_BUFFER_ERROR
} GstOMXAcquireBufferReturn;

struct _GstOMXCore {
  /* Handle to the OpenMAX IL core shared library */
  GModule *module;

#ifdef HAVE_HYBRIS
  void *hybris_module;
#endif

  /* Current number of users, transitions from/to 0
   * call init/deinit */
  GMutex *lock;
  gint user_count; /* LOCK */

  /* OpenMAX core library functions, protected with LOCK */
  OMX_ERRORTYPE (*init) (void);
  OMX_ERRORTYPE (*deinit) (void);
  OMX_ERRORTYPE (*get_handle) (OMX_HANDLETYPE * handle,
      OMX_STRING name, OMX_PTR data, OMX_CALLBACKTYPE * callbacks);
  OMX_ERRORTYPE (*free_handle) (OMX_HANDLETYPE handle);
};

typedef enum {
  GST_OMX_MESSAGE_STATE_SET,
  GST_OMX_MESSAGE_FLUSH,
  GST_OMX_MESSAGE_ERROR,
  GST_OMX_MESSAGE_PORT_ENABLE,
  GST_OMX_MESSAGE_PORT_SETTINGS_CHANGED,
  GST_OMX_MESSAGE_BUFFER_DONE,
} GstOMXMessageType;

struct _GstOMXMessage {
  GstOMXMessageType type;

  union {
    struct {
      OMX_STATETYPE state;
    } state_set;
    struct {
      OMX_U32 port;
    } flush;
    struct {
      OMX_ERRORTYPE error;
    } error;
    struct {
      OMX_U32 port;
      OMX_BOOL enable;
    } port_enable;
    struct {
      OMX_U32 port;
    } port_settings_changed;
    struct {
      OMX_HANDLETYPE component;
      OMX_PTR app_data;
      OMX_BUFFERHEADERTYPE *buffer;
      OMX_BOOL empty;
    } buffer_done;
  } content;
};

struct _GstOMXPort {
  GstOMXComponent *comp;
  guint32 index;

  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  GPtrArray *buffers; /* Contains GstOMXBuffer* */
  GQueue pending_buffers; /* Contains GstOMXBuffer* */
  /* If TRUE we need to get the new caps of this port */
  gboolean settings_changed;
  gboolean flushing;
  gboolean flushed; /* TRUE after OMX_CommandFlush was done */
  gboolean enabled_changed; /* TRUE after OMX_Command{En,Dis}able was done */

  /* Increased whenever the settings of these port change.
   * If settings_cookie != configured_settings_cookie
   * the port has to be reconfigured.
   */
  gint settings_cookie;
  gint configured_settings_cookie;
  gint resurrection_cookie;
};

struct _GstOMXComponent {
  GstObject *parent;
  OMX_HANDLETYPE handle;
  GstOMXCore *core;

  guint64 hacks; /* Flags, GST_OMX_HACK_* */

  /* Added once, never changed. No locks necessary */
  GPtrArray *ports; /* Contains GstOMXPort* */
  gint n_in_ports, n_out_ports;

  /* Locking order: lock -> messages_lock
   *
   * Never hold lock while waiting for messages_cond
   * Always check that messages is empty before waiting */
  GMutex *lock;

  GQueue messages; /* Queue of GstOMXMessages */
  GMutex *messages_lock;
  GCond *messages_cond;

  GMutex resurrection_lock;

  OMX_STATETYPE state;
  /* OMX_StateInvalid if no pending state */
  OMX_STATETYPE pending_state;
  /* OMX_ErrorNone usually, if different nothing will work */
  OMX_ERRORTYPE last_error;

  gint have_pending_reconfigure_outports; /* atomic */
  GList *pending_reconfigure_outports;

  GstGralloc *gralloc;
  int android_buffer_usage;
};

struct _GstOMXBuffer {
  GstOMXPort *port;
  OMX_BUFFERHEADERTYPE *omx_buf;

  /* TRUE if the buffer is used by the port, i.e.
   * between {Empty,Fill}ThisBuffer and the callback
   */
  gboolean used;

  /* Cookie of the settings when this buffer was allocated */
  gint settings_cookie;
  gint resurrection_cookie;

  buffer_handle_t android_handle;
  GstNativeBuffer *native_buffer;
};

extern GQuark     gst_omx_element_name_quark;

GKeyFile *        gst_omx_get_configuration (void);

const gchar *     gst_omx_error_to_string (OMX_ERRORTYPE err);
guint64           gst_omx_parse_hacks (gchar ** hacks);

GstOMXCore *      gst_omx_core_acquire (const gchar * filename);
#ifdef HAVE_HYBRIS
GstOMXCore *      gst_omx_core_acquire_hybris (const gchar * filename);
#endif

void              gst_omx_core_release (GstOMXCore * core);


GstOMXComponent * gst_omx_component_new  (GstObject *parent, const gchar * core_name, const gchar * component_name, const gchar *component_role, guint64 hacks);
void              gst_omx_component_free (GstOMXComponent * comp);

OMX_ERRORTYPE     gst_omx_component_set_state (GstOMXComponent * comp, OMX_STATETYPE state);
OMX_STATETYPE     gst_omx_component_get_state (GstOMXComponent * comp, GstClockTime timeout);

void              gst_omx_component_set_last_error (GstOMXComponent * comp, OMX_ERRORTYPE err);
OMX_ERRORTYPE     gst_omx_component_get_last_error (GstOMXComponent * comp);
const gchar *     gst_omx_component_get_last_error_string (GstOMXComponent * comp);

GstOMXPort *      gst_omx_component_add_port (GstOMXComponent * comp, guint32 index);
GstOMXPort *      gst_omx_component_get_port (GstOMXComponent * comp, guint32 index);

void              gst_omx_component_trigger_settings_changed (GstOMXComponent * comp, guint32 port_index);

OMX_ERRORTYPE     gst_omx_component_get_parameter (GstOMXComponent * comp, OMX_INDEXTYPE index, gpointer param);
OMX_ERRORTYPE     gst_omx_component_set_parameter (GstOMXComponent * comp, OMX_INDEXTYPE index, gpointer param);

OMX_ERRORTYPE     gst_omx_component_get_config (GstOMXComponent * comp, OMX_INDEXTYPE index, gpointer config);
OMX_ERRORTYPE     gst_omx_component_set_config (GstOMXComponent * comp, OMX_INDEXTYPE index, gpointer config);


void              gst_omx_port_get_port_definition (GstOMXPort * port, OMX_PARAM_PORTDEFINITIONTYPE * port_def);
gboolean          gst_omx_port_update_port_definition (GstOMXPort *port, OMX_PARAM_PORTDEFINITIONTYPE *port_definition);

GstOMXAcquireBufferReturn gst_omx_port_acquire_buffer (GstOMXPort *port, GstOMXBuffer **buf);
OMX_ERRORTYPE     gst_omx_port_release_buffer (GstOMXPort *port, GstOMXBuffer *buf);

OMX_ERRORTYPE     gst_omx_port_set_flushing (GstOMXPort *port, gboolean flush);
gboolean          gst_omx_port_is_flushing (GstOMXPort *port);

OMX_ERRORTYPE     gst_omx_port_allocate_buffers (GstOMXPort *port);
OMX_ERRORTYPE     gst_omx_port_deallocate_buffers (GstOMXPort *port);

OMX_ERRORTYPE     gst_omx_port_reconfigure (GstOMXPort * port);

OMX_ERRORTYPE     gst_omx_port_set_enabled (GstOMXPort * port, gboolean enabled);
gboolean          gst_omx_port_is_enabled (GstOMXPort * port);

OMX_ERRORTYPE     gst_omx_port_manual_reconfigure (GstOMXPort * port, gboolean start);

G_END_DECLS

#endif /* __GST_OMX_H__ */
