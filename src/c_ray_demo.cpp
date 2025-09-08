#include <iostream>
#include <vector>
#include <array>
#include <deque>
#include <chrono>
#include <sm/flags>

#include <sampleConfig.h>

#include "MulticamScene.h"
#include "libEyeRenderer.h"

#include "eye3dvisual.h"
#include "fpsprofiler.h"
#include <mplot/compoundray/interop.h> // mathplot <--> compoundray interoperability

#include <mplot/compoundray/EyeVisual.h>
#include <mplot/CoordArrows.h>

// The compound-ray scene exists at global scope in libEyeRenderer.so
extern MulticamScene* cr_scene;

// When the program starts, how many samples per ommatidium/element do you want?
constexpr int samples_per_omm_default = 64;

namespace eye3d
{
    // Your application-specific help message
    void printHelp()
    {
        std::cout << "USAGE:\neye3d -f <path to gltf scene>" << std::endl << std::endl;
        std::cout << "\t-h\tDisplay this help information." << std::endl;
        std::cout << "\t-f\tPath to a gltf scene file (absolute or relative to current "
                  << "working directory, e.g. './data/axis_coloured_blocks.gltf')." << std::endl;
    }
    // Helper to plot coords
    mplot::CoordArrows<>* plot_axes (mplot::Visual<>* thevisual)
    {
        auto cavm = std::make_unique<mplot::CoordArrows<>> (sm::vec<float>{0.0f});
        thevisual->bindmodel (cavm);
        cavm->em = 0.0f; // labels don't work so well
        cavm->finalize();
        return thevisual->addVisualModel (cavm);
    }
    // Flags class
    enum class options : uint32_t
    {
        blender_axes,     // Set true to transform glTF into Blender's z-up axes
        keep_moving,      // If true, movements keep moving
        max_fps,          // If true, poll, instead of fps
        can_exit          // Can exit the program
    };
    // Parse cmd line to find the path and set options
    std::string parse_inputs (int argc, char* argv[], sm::flags<eye3d::options>& opts)
    {
        std::string path = "";
        for (int i=0; i<argc; i++) {
            std::string arg = std::string(argv[i]);
            if (arg == "-h") {
                eye3d::printHelp();
                opts |= eye3d::options::can_exit;
            } else if (arg == "-f") {
                i++;
                path = std::string(argv[i]);
            } else if (arg == "-b") {
                opts |= eye3d::options::blender_axes;
            } else if (arg == "-x") {
                opts |= eye3d::options::max_fps;
            }
        }
        if (path.empty()) {
            eye3d::printHelp();
            opts |= eye3d::options::can_exit;
        }
        return path;
    }
} // namespace eye3d

int main (int argc, char* argv[])
{
    using mc = sm::mathconst<float>;

    // Program options and boolean state
    sm::flags<eye3d::options> opts;
    std::string path = eye3d::parse_inputs (argc, argv, opts);
    if (opts.test (eye3d::options::can_exit)) { return 1; }

    // Boilerplate memory alloc for compound-ray
    multicamAlloc();

    std::vector<sm::vec<float, 3>> ommatidiaPositions;
    std::vector<std::array<float, 3>> ommatidiaData;
    std::vector<Ommatidium>* ommatidia = nullptr;

    // Turn off verbose logging
    setVerbosity (false);
    // Load the file
    std::cout << "Loading glTF file \"" << path << "\"..." << std::endl;
    loadGlTFscene (path.c_str(), (opts.test(eye3d::options::blender_axes)
                                  ? mplot::compoundray::blender_transform() : sutil::Matrix4x4::identity()));

    // Create a mathplot window to render the eye/sensor
    eye3dvisual v (2000, 1200, "Eye 3D (mathplot graphics)", opts.test(eye3d::options::blender_axes));
    v.showCoordArrows (true);
    // Choose how fast the camera should move for key press and mouse events
    v.speed = 0.05f;
    v.angularSpeed = 2.0f * mc::two_pi / 360.0f;
    v.scenetrans_stepsize = 0.1f;

    v.showUserFrame (false);
    v.options.set (mplot::visual_options::rotateAboutSceneOrigin, false);
    v.options.set (mplot::visual_options::highlightCentralVM, true);
    v.options.set (mplot::visual_options::boundingBoxesToJson, true);

    // Use a FPS profiling with a text object on screen
    demo::fps::profiler fps_profiler;
    mplot::VisualTextModel<>* fps_label;
    v.addLabel ("0 FPS", {0.63f, -0.43f, 0.0f}, fps_label);

    // We get the eye data path from the glTF file
    std::string efpath("");
    int ncam = static_cast<int>(getCameraCount());
    int num_compound_cameras = 0;
    int my_compound_camera = -1;
    for (int ci = 0; ci < ncam; ++ci) {
        gotoCamera (ci);
        efpath = getEyeDataPath();
        if (!efpath.empty()) {
            ++num_compound_cameras;
            my_compound_camera = ci;
        }
    }
    if (num_compound_cameras > 1) {
        throw std::runtime_error ("This program works for only one compound eye camera in your gltf.");
    }
    // Now switch to our compound ray camera and set the samples per ommatidium/element
    if (my_compound_camera != -1) {
        gotoCamera (my_compound_camera);
        int csamp = getCurrentEyeSamplesPerOmmatidium();
        std::cout << "Current eye samples per ommatidium is " << csamp << std::endl;
        if (csamp < 32000) { changeCurrentEyeSamplesPerOmmatidiumBy (samples_per_omm_default - csamp); }
    }

    // We get the initial camera localspace. This also serves to reset the camera pose. This is set in the GLTF file.
    sm::mat44<float> initial_camera_space = mplot::compoundray::getCameraSpace (cr_scene);

    // Plot the visual models
    mplot::compoundray::scene_to_visualmodels (cr_scene, &v);

    // Create an EyeVisual 'eye' in our mathplot cr_scene, v.
    sm::vec<float, 3> offset = {};
    auto eyevm = std::make_unique<mplot::compoundray::EyeVisual<>> (offset, &ommatidiaData, ommatidia);
    v.bindmodel (eyevm);
    eyevm->setViewMatrix (initial_camera_space);
    eyevm->finalize();
    mplot::compoundray::EyeVisual<>* eyevm_ptr = v.addVisualModel (eyevm);

    // Make CoordArrows axes to show our camera's localspace
    mplot::CoordArrows<>* cam_cs_ptr = eye3d::plot_axes (&v);
    cam_cs_ptr->setViewMatrix (initial_camera_space);

    // We keep a track of the eye size. Used in subr_detect_camera_changes
    size_t last_eye_size = 0u;

    /**
     * Subroutine lambda: Detect changes in the camera (there can be multiple cameras, some
     * compound ray, some non-compound ray). The complexity here results from this complexity in
     * compound-ray
     */
    auto subr_detect_camera_changes = [&v, &ommatidia, &ommatidiaData, &ommatidiaPositions,
                                       &last_eye_size, &eyevm_ptr, opts] ()
    {
        size_t curr_eye_size = last_eye_size;
        // Detect changes in the camera and update eye model as necessary
        if (ommatidiaData.size() == 0) {
            if (isCompoundEyeActive()) { getCameraData (ommatidiaData); }
        } // else no need to re-get data

        // Change showing the 'cones' of the compound eye visual model?
        if (eyevm_ptr->show_cones != v.vstate.test(eye3dvisual::state::show_cones)) {
            eyevm_ptr->show_cones = v.vstate.test(eye3dvisual::state::show_cones);
            eyevm_ptr->reinit();
        }
        // Change the length of the cones?
        if (eyevm_ptr->get_cone_length() != v.manual_cone_length) {
            eyevm_ptr->set_cone_length (v.manual_cone_length);
        }
        // Update eyevm model (or just update colours)
        eyevm_ptr->ommatidia = ommatidia;

        if (ommatidia != nullptr) {
            curr_eye_size = ommatidia->size();
            if (curr_eye_size != last_eye_size) {
                eyevm_ptr->reinit();
                last_eye_size = curr_eye_size;
            } else {
                eyevm_ptr->reinitColours(); // 4x faster to just reinitColours
            }
        }
    };

    /**
     * Subroutine: Move the camera according to key events in the mathplot window
     */
    auto subr_key_move_camera = [&v, &eyevm_ptr, &cam_cs_ptr, &initial_camera_space, opts]()
    {
        cam_cs_ptr->setHide (!v.vstate.test(eye3dvisual::state::show_camframe));

        if (v.isActivelyMoving()) {
            sm::vec<float, 3> t = v.getMovementVector (opts.test(eye3d::options::keep_moving));
            translateCamerasLocally (t.x(), t.y(), t.z());
            // Up-down (pitch) is rotation about local camera frame axis x
            rotateCamerasLocallyAround (v.getVerticalRotationAngle (opts.test(eye3d::options::keep_moving)), 1.0f, 0.0f, 0.0f);
            // Left-and-right (yaw) is rotation about local camera frame axis y
            rotateCamerasLocallyAround (v.getHorizontalRotationAngle (opts.test(eye3d::options::keep_moving)), 0.0f, 1.0f, 0.0f);
            // Roll
            rotateCamerasLocallyAround (v.getRollRotationAngle (opts.test(eye3d::options::keep_moving)), 0.0f, 0.0f, 1.0f);
        }

        // Get the camera space and update our eye and camera-frame models
        sm::mat44<float> camera_space = mplot::compoundray::getCameraSpace (cr_scene);

        // reset to initial camera space if requested
        if (v.vstate.test (eye3dvisual::state::campose_reset_request) == true) {
            setCameraPoseMatrix (mplot::compoundray::mat44_to_Matrix4x4 (initial_camera_space));
            v.stop(); // cancel any active movements
            camera_space = initial_camera_space;
            v.vstate.reset (eye3dvisual::state::campose_reset_request);
        }

        // Update the view matrix of eye and eye localspace axes
        eyevm_ptr->setViewMatrix (camera_space);
        cam_cs_ptr->setViewMatrix (camera_space);
    };

    /**
     * The main program loop
     */

    while (!v.readyToFinish()) {

        // Tell the fps_profiler that we're at the start of a loop
        fps_profiler.at_begin (getCurrentEyeSamplesPerOmmatidium());
        fps_label->setupText (fps_profiler.fps_txt);
        // The current camera may have changed, this subroutine deals with any changes
        subr_detect_camera_changes();
        // Now render the mathplot window
        v.render();
        // Save some electricity while developing - limit to 60 FPS. For max speed use v.poll() (-x)
        if (opts.test (eye3d::options::max_fps)) { v.poll(); } else { v.waitevents (0.018); }
        // Deal with any movements commanded by key press events (including reset)
        subr_key_move_camera();
        // Do the compound-ray ray casting to recompute the scene
        renderFrame();
        // Access data so that a brain model could be fed
        if (isCompoundEyeActive()) {
            getCameraData (ommatidiaData);
            ommatidia = &cr_scene->m_ommVecs[cr_scene->getCameraIndex()];
        }
        // Mark that we got to the end of the loop
        fps_profiler.at_end();
    }

    stop(); // stop compound-ray from running
    multicamDealloc(); // De-allocate compound-ray memory

    return 0;
}
