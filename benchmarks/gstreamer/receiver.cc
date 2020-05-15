#include <atomic>
#include <gst/gst.h>
#include <gst/app/app.h>

// Share frame between main loop and gstreamer callback
/* std::atomic<cv::Mat*> atomicFrame; */

/**
 * @brief Check preroll to get a new frame using callback
 *  https://gstreamer.freedesktop.org/documentation/design/preroll.html
 * @return GstFlowReturn
 */
GstFlowReturn new_preroll(GstAppSink* /*appsink*/, gpointer /*data*/)
{
    return GST_FLOW_OK;
}

/**
 * @brief This is a callback that get a new frame when a preroll exist
 *
 * @param appsink
 * @return GstFlowReturn
 */
GstFlowReturn new_sample(GstAppSink *appsink, gpointer /*data*/)
{
    static int framecount = 0;

    // Get caps and frame
    GstSample *sample = gst_app_sink_pull_sample(appsink);
    GstCaps *caps = gst_sample_get_caps(sample);
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const int width = g_value_get_int(gst_structure_get_value(structure, "width"));
    const int height = g_value_get_int(gst_structure_get_value(structure, "height"));

    // Print dot every 30 frames
    if(!(framecount%30)) {
        g_print(".");
    }

    // Show caps on first frame
    if(!framecount) {
        g_print("caps: %s\n", gst_caps_to_string(caps));
    }
    framecount++;

    // Get frame data
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ);

    // Convert gstreamer data to OpenCV Mat
    /* cv::Mat* prevFrame; */
    /* prevFrame = atomicFrame.exchange(new cv::Mat(cv::Size(width, height), CV_8UC3, (char*)map.data, cv::Mat::AUTO_STEP)); */
    /* if(prevFrame) { */
    /*     delete prevFrame; */
    /* } */

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

/**
 * @brief Bus callback
 *  Print important messages
 *
 * @param bus
 * @param message
 * @param data
 * @return gboolean
 */
static gboolean my_bus_callback(GstBus *bus, GstMessage *message, gpointer data)
{
    // Debug message
    //g_print("Got %s message\n", GST_MESSAGE_TYPE_NAME(message));
    switch(GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug;

            gst_message_parse_error(message, &err, &debug);
            g_print("Error: %s\n", err->message);
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS:
            /* end-of-stream */
            break;
        default:
            /* unhandled message */
            break;
    }
    /* we want to be notified again the next time there is a message
     * on the bus, so returning TRUE (FALSE means we want to stop watching
     * for messages on the bus and our callback should not be called again)
     */
    return true;
}

int main(int argc, char **argv)
{
    gst_init(&argc, &argv);

#ifdef WWWW
    gchar *descr = g_strdup(
        "udpsrc port=8888 "
        "! application/x-rtp, payload=96 ! rtph265depay ! h264parse ! avdec_h264 "
        "! decodebin ! videoconvert ! video/x-raw,format=(string)BGR ! videoconvert "
        "! appsink name=sink emit-signals=true sync=false max-buffers=1 drop=true"
    );
#else
    gchar *descr = g_strdup(
        "udpsrc port=8888 ! application/x-rtp, payload=96 ! rtph265depay ! h265parse"
    );
#endif

    GError *error        = nullptr;
    GstElement *pipeline = gst_parse_launch(descr, &error);

    if (error) {
        g_print("could not construct pipeline: %s\n", error->message);
        g_error_free(error);
        exit(-1);
    }

    // Get sink
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");

    /**
     * @brief Get sink signals and check for a preroll
     *  If preroll exists, we do have a new frame
     */
    gst_app_sink_set_emit_signals((GstAppSink*)sink, true);
    gst_app_sink_set_drop((GstAppSink*)sink, true);
    gst_app_sink_set_max_buffers((GstAppSink*)sink, 1);
    GstAppSinkCallbacks callbacks = { nullptr, new_preroll, new_sample };
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, nullptr, nullptr);

    // Declare bus
    GstBus *bus;
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_add_watch(bus, my_bus_callback, nullptr);
    gst_object_unref(bus);

    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);

    // Main loop
    while(1) {
        g_main_iteration(false);

        /* cv::Mat* frame = atomicFrame.load(); */
        /* if(frame) { */
        /*     cv::imshow("Frame", atomicFrame.load()[0]); */
        /*     cv::waitKey(30); */
        /* } */
    }

    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));

    return 0;
}
