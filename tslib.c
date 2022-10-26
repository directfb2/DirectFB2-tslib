/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#define DFB_INPUTDRIVER_HAS_SET_CONFIGURATION

#include <core/input_driver.h>
#include <direct/thread.h>
#include <fusion/vector.h>
#include <tslib.h>

D_DEBUG_DOMAIN( Tslib, "Input/tslib", "tslib Input Driver" );

DFB_INPUT_DRIVER( tslib )

/**********************************************************************************************************************/

typedef struct {
     CoreInputDevice     *device;

     struct tsdev        *ts;
     struct ts_sample_mt *samp;

     int                  max_slots;

     struct ts_sample    *old_samp;

     DirectThread        *thread;
} tslibData;

#define MAX_TSLIB_DEVICES 16
#define MAX_TSLIB_SLOTS   10

/* Touchscreen devices are stored in the device_names array. */
static char *device_names[MAX_TSLIB_DEVICES];

/* Number of entries in the device_names array. */
static int   num_devices = 0;

/**********************************************************************************************************************/

static void
config_values_parse( FusionVector *vector,
                     const char   *arg )
{
     char *values = D_STRDUP( arg );
     char *s      = values;
     char *r, *p  = NULL;

     if (!values) {
          D_OOM();
          return;
     }

     while ((r = direct_strtok_r( s, ",", &p ))) {
          direct_trim( &r );

          r = D_STRDUP( r );
          if (!r)
               D_OOM();
          else
               fusion_vector_add( vector, r );

          s = NULL;
     }

     D_FREE( values );
}

static void
config_values_free( FusionVector *vector )
{
     char *value;
     int   i;

     fusion_vector_foreach (value, i, *vector)
          D_FREE( value );

     fusion_vector_destroy( vector );
}

/**********************************************************************************************************************/

static void *
tslib_event_thread( DirectThread *thread,
                    void         *arg )
{
     tslibData *data = arg;
     int        i;

     D_DEBUG_AT( Tslib, "%s()\n", __FUNCTION__ );

     while (ts_read_mt( data->ts, &data->samp, data->max_slots, 1 ) == 1) {
          DFBInputEvent evt = { .type = DIET_UNKNOWN };

          for (i = 0; i < data->max_slots; i++) {
               if (data->samp[i].valid < 1)
                    continue;

               if (data->samp[i].pressure) {
                    if (data->samp[i].x != data->old_samp[i].x) {
                         evt.type    = DIET_AXISMOTION;
                         evt.flags   = DIEF_AXISABS | DIEF_BUTTONS;
                         evt.axis    = DIAI_X;
                         evt.axisabs = data->samp[i].x;
                         evt.buttons = DIBM_LEFT;
                         evt.slot_id = i;

                         dfb_input_dispatch( data->device, &evt );

                         data->old_samp[i].x = data->samp[i].x;
                    }

                    if (data->samp[i].y != data->old_samp[i].y) {
                         evt.type    = DIET_AXISMOTION;
                         evt.flags   = DIEF_AXISABS | DIEF_BUTTONS;
                         evt.axis    = DIAI_Y;
                         evt.axisabs = data->samp[i].y;
                         evt.buttons = DIBM_LEFT;
                         evt.slot_id = i;

                         dfb_input_dispatch( data->device, &evt );

                         data->old_samp[i].y = data->samp[i].y;
                    }
               }
               else {
                    data->old_samp[i].x = -1;
                    data->old_samp[i].y = -1;
               }

               if (data->samp[i].pressure != data->old_samp[i].pressure) {
                    evt.type    = data->samp[i].pressure ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                    evt.flags   = DIEF_NONE;
                    evt.button  = DIBI_LEFT;
                    evt.slot_id = i;

                    dfb_input_dispatch( data->device, &evt );

                    data->old_samp[i].pressure = data->samp[i].pressure;
               }
          }
     }

     D_DEBUG_AT( Tslib, "Tslib Event thread terminated\n" );

     return NULL;
}

static bool
check_device( const char *device )
{
     struct tsdev *ts;

     D_DEBUG_AT( Tslib, "%s( '%s' )\n", __FUNCTION__, device );

     ts = ts_open( device, 0 );
     if (!ts) {
          D_DEBUG_AT( Tslib, "  -> open failed!\n" );
          return false;
     }

     if (ts_config( ts )) {
          D_DEBUG_AT( Tslib, "  -> configure failed!\n" );
          ts_close( ts );
          return false;
     }

     ts_close( ts );

     return true;
}

/**********************************************************************************************************************/

static int
driver_get_available()
{
     const char   *value;
     FusionVector  tslib_devices;
     int           i;
     char         *tsdev;
     char          buf[32];

     if (num_devices) {
          for (i = 0; i < num_devices; i++) {
               D_FREE( device_names[i] );
               device_names[i] = NULL;
          }

          num_devices = 0;

          return 0;
     }

     /* Use the devices specified in the configuration. */
     if ((value = direct_config_get_value( "tslib-devices" ))) {
          const char *device;

          fusion_vector_init( &tslib_devices, 2, NULL );

          config_values_parse( &tslib_devices, value );

          fusion_vector_foreach (device, i, tslib_devices) {
               if (num_devices >= MAX_TSLIB_DEVICES)
                    break;

               /* Update the device_names. */
               if (check_device( device )) {
                    D_ASSERT( device_names[num_devices] == NULL );
                    device_names[num_devices++] = D_STRDUP( device );
               }
          }

          config_values_free( &tslib_devices );

          return num_devices;
     }

     /* No devices specified. Try to guess some, check for environment variable. */
     tsdev = getenv( "TSLIB_TSDEVICE" );
     if (tsdev && check_device( tsdev ))
          device_names[num_devices++] = D_STRDUP( tsdev );

     for (i = 0; i < MAX_TSLIB_DEVICES; i++) {
          if (num_devices >= MAX_TSLIB_DEVICES)
               break;

          snprintf( buf, sizeof(buf), "/dev/input/event%d", i );

          if (tsdev && !strcmp( tsdev, buf ))
               continue;

          /* Update the device_names array. */
          if (check_device( buf )) {
               D_ASSERT( device_names[num_devices] == NULL );
               device_names[num_devices++] = D_STRDUP( buf );
          }
     }

     return num_devices;
}

static void
driver_get_info( InputDriverInfo *info )
{
     info->version.major = 0;
     info->version.minor = 1;

     snprintf( info->name,   DFB_INPUT_DRIVER_INFO_NAME_LENGTH,   "tslib" );
     snprintf( info->vendor, DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "DirectFB" );
}

static DFBResult
driver_open_device( CoreInputDevice  *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     tslibData    *data;
     struct tsdev *ts;
     int           i;

     D_DEBUG_AT( Tslib, "%s()\n", __FUNCTION__ );

     /* Open device. */
     ts = ts_open( device_names[number], 0 );
     if (!ts) {
          D_ERROR( "Input/tslib: Could not open device '%s'!\n", device_names[number] );
          return DFB_INIT;
     }

     /* Configure device. */
     if (ts_config( ts )) {
          D_ERROR( "Input/tslib: Could not configure device!\n" );
          ts_close( ts );
          return DFB_INIT;
     }

     /* Fill device information. */
     info->prefered_id     = DIDID_MOUSE;
     info->desc.type       = DIDTF_MOUSE;
     info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
     info->desc.max_axis   = DIAI_Y;
     info->desc.max_button = DIBI_LEFT;
     snprintf( info->desc.name,   DFB_INPUT_DEVICE_DESC_NAME_LENGTH,   "Touchscreen" );
     snprintf( info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "Tslib" );

     /* Allocate and fill private data. */
     data = D_CALLOC( 1, sizeof(tslibData) );
     if (!data) {
          ts_close( ts );
          return D_OOM();
     }

     data->device = device;
     data->ts     = ts;

     data->samp = D_CALLOC( MAX_TSLIB_SLOTS, sizeof(struct ts_sample_mt) );
     if (!data->samp) {
          D_OOM();
          goto error;
     }

     data->max_slots = 1;

     data->old_samp = D_CALLOC( MAX_TSLIB_SLOTS, sizeof(struct ts_sample) );
     if (!data->old_samp) {
          D_OOM();
          goto error;
     }

     for (i = 0; i < MAX_TSLIB_SLOTS; i++) {
          data->old_samp[i].x = -1;
          data->old_samp[i].y = -1;
     }

     /* Start tslib event thread. */
     data->thread = direct_thread_create( DTT_INPUT, tslib_event_thread, data, "Tslib Event" );

     *driver_data = data;

     return DFB_OK;

error:
     ts_close( ts );
     D_FREE( data );

     return DFB_INIT;
}

static DFBResult
driver_get_keymap_entry( CoreInputDevice           *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry )
{
     return DFB_UNSUPPORTED;
}

static void
driver_close_device( void *driver_data )
{
     tslibData *data = driver_data;

     D_DEBUG_AT( Tslib, "%s()\n", __FUNCTION__ );

     /* Terminate the tslib event thread. */
     direct_thread_cancel( data->thread );

     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     if (data->old_samp)
          D_FREE( data->old_samp );

     if (data->samp)
          D_FREE( data->samp );

     ts_close( data->ts );

     D_FREE( data );
}

/**********************************************************************************************************************
 ********************************* Set configuration function *********************************************************
 **********************************************************************************************************************/

static DFBResult
driver_set_configuration( CoreInputDevice            *device,
                          void                       *driver_data,
                          const DFBInputDeviceConfig *config )
{
     tslibData *data = driver_data;

     D_DEBUG_AT( Tslib, "%s()\n", __FUNCTION__ );

     if (config->flags & DIDCONF_MAX_SLOTS) {
          data->max_slots = config->max_slots;

          if (data->max_slots > MAX_TSLIB_SLOTS)
               return DFB_INVARG;
     }

     return DFB_OK;
}
