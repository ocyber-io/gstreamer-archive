#include <gst/gst.h>
#include <gst/gstpad.h>
#include <gst/rtsp/gstrtsp.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

typedef struct _CustomData {
    GstElement *streaming_pipe;
    GstElement *src;
    GstElement *depay;
    GstElement *decoder;
    GstElement *video_convert;
    GstElement *encoder;
    GstElement *sink;
    GMainLoop *m_loop;
    gboolean change_url;
    gboolean url1;
    clock_t startT;
} CustomData;

static void on_rtsp_pad_added(GstElement *element, GstPad *new_pad, CustomData *data) {
    gchar *name;
    GstCaps *caps;

    caps = gst_caps_from_string("application/x-rtp");
    name = gst_pad_get_name(new_pad);

    if (!gst_element_link_pads(element, name, data->depay, "sink")) {
        g_print("\npad_added: failed to link elements"); //ERROR when linking the new rtspsrc after breaking up the pipeline
    }
    g_free(name);
    data->startT = clock();
}

static GstPadProbeReturn event_probe(GstPad *pad, GstPadProbeInfo *info, CustomData *data) {
    GstElement *rtspsrcOld, *rtspsrcNew, *depay;

    if (GST_EVENT_TYPE(GST_PAD_PROBE_INFO_DATA(info)) != GST_EVENT_EOS) {
        g_print("\nNot an EOS event; pass probe return");
        return GST_PAD_PROBE_PASS;
    }

    gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info));

    rtspsrcOld = gst_bin_get_by_name(GST_BIN(data->streaming_pipe), "rtspsrc");
    if (rtspsrcOld) {
        depay = gst_bin_get_by_name(GST_BIN(data->streaming_pipe), "depay");
        gst_element_unlink(rtspsrcOld, depay);
        gst_bin_remove(GST_BIN(data->streaming_pipe), rtspsrcOld); //remove old rtspsrc from pipeline, should unlink from depay automatically.
        rtspsrcNew = gst_element_factory_make("rtspsrc", "rtspsrc123");
        g_object_set(rtspsrcNew, "location", "rtsp://127.0.0.1:8555/new_source", "latency", 0, NULL);
        g_signal_connect(rtspsrcNew, "pad-added", G_CALLBACK(on_rtsp_pad_added), data);

        gst_bin_add(GST_BIN(data->streaming_pipe), rtspsrcNew);
        gst_element_link(data->depay, rtspsrcNew); // Link depay to the new rtspsrc
        gst_element_set_state(GST_ELEMENT(data->streaming_pipe), GST_STATE_PLAYING);
        g_print("\nSet playing\n");

        return GST_PAD_PROBE_DROP;
    }

    return GST_PAD_PROBE_DROP;
}

static GstPadProbeReturn cb_have_data(GstPad *pad, GstPadProbeInfo *info, CustomData *data) {
    g_print("\nPROBE CALLBACK!");
    g_print("Time: %f", ((double)(clock() - data->startT)) / CLOCKS_PER_SEC);
    if (((double)(clock() - data->startT)) / CLOCKS_PER_SEC > 30.0) {
        data->change_url = true;
        data->startT = clock();
    }
    if (data->change_url) {
        g_print("\nIF PROBE CALLBACK!");
        GstPad *srcPad, *sinkPad;

        srcPad = gst_element_get_static_pad(data->decoder, "src");
        gst_pad_remove_probe(pad, GST_PAD_PROBE_INFO_ID(info)); // Remove the probe from the original pad
        gst_pad_add_probe(srcPad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                          (GstPadProbeCallback)event_probe, data, NULL); // Add the probe to the new pad

        // Push EOS into the element, wait for the EOS to appear on the srcpad
        sinkPad = gst_element_get_static_pad(data->depay, "sink");
        gst_pad_send_event(sinkPad, gst_event_new_eos());

        data->change_url = false;
    }

    return GST_PAD_PROBE_OK;
}

int main(int argc, char *argv[]) {
    /* Initialize GStreamer */
    gst_init(&argc, &argv);
    CustomData data;
    GstStateChangeReturn ret;
    GstPad *pad;

    data.m_loop = g_main_loop_new(NULL, FALSE);

    // Create pipeline elements
    data.streaming_pipe = gst_pipeline_new("display_pipeline");
    data.src = gst_element_factory_make("rtspsrc", "rtspsrc");
    data.depay = gst_element_factory_make("rtph264depay", "depay");
    data.decoder = gst_element_factory_make("avdec_h264", "decoder");
    data.video_convert = gst_element_factory_make("videoconvert", "video_convert");
    data.encoder = gst_element_factory_make("x264enc", "encoder");
    data.sink = gst_element_factory_make("rtspclientsink", NULL);
    data.change_url = false;
    data.url1 = false;

    if (!(data.streaming_pipe || data.src || data.depay || data.decoder || data.video_convert || data.encoder || data.sink)) {
        g_print("Could not create pipeline elements");
        return 1;
    }

    g_object_set(G_OBJECT(data.src), "location", "rtsp://127.0.0.1:8555/test", "latency", 0, NULL);
    g_object_set(G_OBJECT(data.sink), "location", "rtsp://127.0.0.1:8555/live", NULL);
//    g_object_set(G_OBJECT(data.sink), "location", "/home/ubuntu/Downloads/v.mp4");
    g_signal_connect(data.src, "pad-added", G_CALLBACK(on_rtsp_pad_added), &data);


    // Add and link elements to create the full pipeline
    gst_bin_add_many(GST_BIN(data.streaming_pipe), data.src, data.depay, data.decoder, data.video_convert, data.encoder, data.sink, NULL);
    if (!gst_element_link_many(data.depay, data.decoder, data.video_convert, data.encoder, data.sink, NULL)) {
        g_print("Cannot link elements");
        return 1;
    }

    pad = gst_element_get_static_pad(data.depay, "src");
    if (pad == NULL) {
        g_print("Could not get static pad");
        return 1;
    }
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)cb_have_data, &data, NULL);
    gst_object_unref(pad);

    ret = gst_element_set_state(data.streaming_pipe, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.streaming_pipe);
        return -1;
    }

    g_main_loop_run(data.m_loop);

    gst_object_unref(data.src);
    gst_object_unref(data.depay);
    gst_object_unref(data.decoder);
    gst_object_unref(data.sink);
    gst_object_unref(data.streaming_pipe);
    g_main_loop_unref(data.m_loop);

    return 0;
}
