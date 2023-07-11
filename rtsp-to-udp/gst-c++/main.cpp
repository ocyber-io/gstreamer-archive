#include <gst/gst.h>

static GMainLoop *loop;

static gboolean my_bus_callback(GstBus *bus, GstMessage *message, gpointer data)
{
    g_print("Got %s message\n", GST_MESSAGE_TYPE_NAME(message));

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug;

            gst_message_parse_error(message, &err, &debug);
            g_print("Error: %s\n", err->message);
            g_error_free(err);
            g_free(debug);

            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_EOS:
            g_print("End of Stream Reached\n");
            g_main_loop_quit(loop);
            break;
        default:
            break;
    }

    return TRUE;
}

int main(int argc, char *argv[])
{
    GstElement *pipeline;
    GstBus *bus;
    guint bus_watch_id;

    /* init */
    gst_init(&argc, &argv);

    pipeline = gst_parse_launch(
            "rtspsrc name=source ! parsebin ! queue ! h264parse ! rndbuffersize ! mpegtsmux ! udpsink name=sink",
            NULL);

    // Access the rtspsrc element from the pipeline
    GstElement *rtspsrc = gst_bin_get_by_name(GST_BIN(pipeline), "source");
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    // Set the properties of rtspsrc using g_object_set
    g_object_set(rtspsrc, "location", "rtsp://127.0.0.1:8555/test", NULL);
    g_object_set(sink, "port", 1234, "host", "127.0.0.1", NULL);


    // Cleanup rtspsrc element
    gst_object_unref(rtspsrc);

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    bus_watch_id = gst_bus_add_watch(bus, my_bus_callback, NULL);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    /* clean up */
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);

    return 0;
}
