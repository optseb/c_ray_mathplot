/*
 * Apply a projection to a HexGrid to place it on a sphere, to make a compound-like eye.
 */

#include <iostream>
#include <fstream>
#include <cmath>

#include <sm/mathconst>
#include <sm/mat44>
#include <sm/scale>
#include <sm/vec>
#include <sm/vvec>
#include <sm/quaternion>
#define HEXGRID_COMPILE_LOAD_AND_SAVE 1 // DO need to save out the HexGrid
#include <sm/hexgrid>
#include <sm/hdfdata>

#include <mplot/Visual.h>
#include <mplot/ScatterVisual.h>
#include <mplot/QuiverVisual.h>
#include <mplot/HexGridVisual.h>

enum class spherical_projection
{
    mercator,
    equirectangular,
    cassini,
    splodge
};

int main()
{
    constexpr spherical_projection proj =  spherical_projection::splodge;

    // You can hide the RGB arrows
    constexpr bool show_rgb = true;

    using mc = sm::mathconst<float>;

    constexpr bool show_version_stdout = false;
    mplot::Visual v(1024, 768, "Hexy Eyes", show_version_stdout);
    v.setSceneTrans (sm::vec<float,3>{0.0f, 0.0f, -1.1f});
    v.userInfoStdout (false);
    v.showCoordArrows (true);
    v.lightingEffects();

    sm::scale<float> clr_scale;
    clr_scale.setParams (1.0f, 0.0f);

    // radius of sphere
    constexpr float r_sph = 1.0f;

    // Position of the each eye model
    sm::vec<float, 3> eyepos = { 0.0f, 0.0f, 0.0f };

    // An x offset used later
    float eye_x_loc = 0.0f;

    // Make a HexGrid of width similar to sphere
    constexpr float hex_d = r_sph / 15.0f;
    constexpr float hex_span = mc::two_pi * r_sph;
    sm::hexgrid hg(hex_d, hex_span, 0.0f);
    // the argument is the circlular boundary radius (0.5 * pi * r_sph) should wrap up to half way round sphere
    if constexpr (proj == spherical_projection::splodge) {
        hg.setCircularBoundary (0.95f * r_sph);
    } else {
        hg.setCircularBoundary (0.5f * mc::pi * r_sph);
    }

    // hg has d_x and d_y. Can make up a new container of 3D locations for each hex.
    sm::vvec<sm::vec<float, 3>> sphere_coords(hg.num());
    for (unsigned int i = 0u; i < hg.num(); ++i) {
        // This is the inverse Mercator projection.
        // See https://stackoverflow.com/questions/12732590/how-map-2d-grid-points-x-y-onto-sphere-as-3d-points-x-y-z
        sm::vec<float, 2> xy = { hg.d_x[i], hg.d_y[i] };

        float longitude = 0.0f; // or lambda
        float latitude = 0.0f;  // or phi
        if constexpr (proj == spherical_projection::equirectangular) {
            float phi0 = 0.0f;
            float phi1 = 0.0f;
            float lambda0 = 0.0f;
            longitude = xy[0] / (r_sph * std::cos(phi1)) + lambda0;
            latitude = xy[1] / r_sph + phi0;
        } else if constexpr (proj == spherical_projection::cassini) {
            // Spherical Cassini
            float phi0 = 0.0f;
            float lambda0 = 0.0f;
            float D = xy[1] / r_sph + phi0;
            longitude = lambda0 + std::atan2 (std::tan(xy[0]/r_sph), std::cos(D));
            latitude = std::asin (std::sin(D) * std::cos(xy[0]/r_sph));

        } else if constexpr (proj == spherical_projection::splodge) {
            // It's all below
        } else { // Default Mercator projection
            longitude = xy[0] / r_sph;
            latitude = 2.0f * std::atan (std::exp(xy[1]/r_sph)) - mc::pi_over_2;
        }

        if constexpr (proj == spherical_projection::splodge) {
            // In the splodge projection we just 'throw' the 2D plane onto a sphere
            sphere_coords[i][1] = xy[0];
            sphere_coords[i][2] = xy[1];
            float z_sq = r_sph * r_sph - xy.sq().sum();
            float z_sph = 0.0f;
            if (z_sq >= 0.0f)  {
                z_sph = -std::sqrt (z_sq);
            } else {
                z_sph = +std::sqrt (-z_sq); // Anything beyond the edge of r_sph
            }
            sm::vec<float> prerotate = { eye_x_loc + z_sph, xy[0], xy[1] };
            sm::quaternion<float> q1;
            q1.rotate (sm::vec<float>{0, 1, 0}, mc::pi_over_6);
            sm::mat44<float> m1;
            m1.rotate (q1);
            sphere_coords[i] = (m1 * prerotate).less_one_dim();
            //sphere_coords[i][0] =  eye_x_loc + z_sph;
        } else { // it's a serious projection
            float coslat = std::cos (latitude);
            float sinlat = std::sin (latitude);
            float coslong = std::cos (longitude);
            float sinlong = std::sin (longitude);
            sphere_coords[i] =  { eye_x_loc + r_sph * coslat * coslong, r_sph * coslat * sinlong , r_sph * sinlat };
        }
    }

    // 'R' neighbours (neighbour east) on the sphere
    sm::vvec<sm::vec<float, 3>> neighb_r(hg.num(), sm::vec<float, 3>{0,0,0});
    sm::vvec<sm::vec<float, 3>> neighb_g(hg.num(), sm::vec<float, 3>{0,0,0});
    sm::vvec<sm::vec<float, 3>> neighb_b(hg.num(), sm::vec<float, 3>{0,0,0});
    for (unsigned int i = 0; i < hg.num(); ++i) {

        if (hg.d_ne[i] != -1) {
            neighb_r[i] = sphere_coords[hg.d_ne[i]] - sphere_coords[i];
        }
        if (hg.d_nne[i] != -1) {
            neighb_g[i] = sphere_coords[hg.d_nne[i]] - sphere_coords[i];
        }
        if (hg.d_nnw[i] != -1) {
            neighb_b[i] = sphere_coords[hg.d_nnw[i]] - sphere_coords[i];
        }
    }

    // The HDF5 data gives ommatidium neighbour information that may be useful in a client program
    hg.save ("hexy_eye_hexgrid.h5");
    {
        sm::hdfdata d("hexy_eye_3d_coords.h5", std::ios::out | std::ios::trunc);
        d.add_contained_vals ("/neighb_r", neighb_r);
        d.add_contained_vals ("/neighb_g", neighb_g);
        d.add_contained_vals ("/neighb_b", neighb_b);
        d.add_contained_vals ("/sphere_coords", sphere_coords);
    }

    // Will call this fn once for the eye
    auto output_coords = [r_sph, hg](const sm::vvec<sm::vec<float, 3>>& coords, sm::vec<float, 3> eyeoffset, std::ofstream& fout) {
        constexpr float focal_offset = r_sph;
        constexpr float radius = r_sph;
        for (unsigned int i = 0; i < coords.size(); ++i) {
            auto norm = coords[i];
            norm.renormalize();
            float acceptance_angle = 1.0f;
            auto c1 = radius * (coords[i] - eyeoffset);
            if (hg.d_ne[i] != -1) {
                auto c2 = radius * (coords[hg.d_ne[i]] - eyeoffset);
                acceptance_angle *= c1.angle(c2);
            } else if (hg.d_nne[i] != -1) {
                auto c2 = radius * (coords[hg.d_nne[i]] - eyeoffset);
                acceptance_angle *= c1.angle(c2);
            } else if (hg.d_nnw[i] != -1) {
                auto c2 = radius * (coords[hg.d_nnw[i]] - eyeoffset);
                acceptance_angle *= c1.angle(c2);
            } else if (hg.d_nw[i] != -1) {
                auto c2 = radius * (coords[hg.d_nw[i]] - eyeoffset);
                acceptance_angle *= c1.angle(c2);
            } else if (hg.d_nsw[i] != -1) {
                auto c2 = radius * (coords[hg.d_nsw[i]] - eyeoffset);
                acceptance_angle *= c1.angle(c2);
            } else if (hg.d_nse[i] != -1) {
                auto c2 = radius * (coords[hg.d_nse[i]] - eyeoffset);
                acceptance_angle *= c1.angle(c2);
            } // else acceptange angle will be unchanged at 1.0f

            std::string ntxt = norm.str_comma_separated(' '); // normals (vertices were already normalized)
            std::string vtxt = (coords[i] * radius).str_comma_separated(' ');
            fout << vtxt << " " << ntxt << " " << acceptance_angle << " " << focal_offset << std::endl;
        }
    };
    std::ofstream fout ("hexy.eye", std::ios::out | std::ios::trunc);
    if (fout.is_open()) {
        output_coords (sphere_coords, sm::vec<float, 3>{eye_x_loc, 0, 0}, fout);
    }
    sm::vvec<float> data;
    data.linspace (0, 1, hg.num());

    // First eye
    constexpr float hex_d_prop = 0.2f;
    auto sv = std::make_unique<mplot::ScatterVisual<float>> (eyepos);
    v.bindmodel (sv);
    sv->setDataCoords (&sphere_coords);
    sv->setScalarData (&data);
    sv->radiusFixed = hex_d * hex_d_prop;
    sv->colourScale = clr_scale;
    sv->cm.setType (mplot::ColourMapType::Jet);
    sv->finalize();
    v.addVisualModel (sv);

    if constexpr (show_rgb) {
        // Eye 1 RGB directions
        sm::vvec<float> clrs (neighb_r.size(), 0.0f); // red
        auto vmp = std::make_unique<mplot::QuiverVisual<float>>(&sphere_coords, eyepos, &neighb_r,
                                                                mplot::ColourMapType::Rainbow);
        v.bindmodel (vmp);
        vmp->scalarData = &clrs;
        vmp->colourScale.compute_scaling (0, 1);
        vmp->do_quiver_length_scaling = false; // Don't (auto)scale the lengths of the vectors
        vmp->quiver_length_gain = 0.5f;        // Apply a fixed gain to the length of the quivers on screen
        vmp->fixed_quiver_thickness = 0.01f/5; // Fixed quiver thickness
        vmp->finalize();
        v.addVisualModel (vmp);

        clrs.set_from (0.33333f); // green
        vmp = std::make_unique<mplot::QuiverVisual<float>>(&sphere_coords, eyepos, &neighb_g,
                                                           mplot::ColourMapType::Rainbow);
        v.bindmodel (vmp);
        vmp->scalarData = &clrs;
        vmp->colourScale.compute_scaling (0, 1);
        vmp->do_quiver_length_scaling = false; // Don't (auto)scale the lengths of the vectors
        vmp->quiver_length_gain = 0.5f;        // Apply a fixed gain to the length of the quivers on screen
        vmp->fixed_quiver_thickness = 0.01f/5; // Fixed quiver thickness
        vmp->finalize();
        v.addVisualModel (vmp);

        clrs.set_from (0.66667f); // blue
        vmp = std::make_unique<mplot::QuiverVisual<float>>(&sphere_coords, eyepos, &neighb_b,
                                                           mplot::ColourMapType::Rainbow);
        v.bindmodel (vmp);
        vmp->scalarData = &clrs;
        vmp->colourScale.compute_scaling (0, 1);
        vmp->do_quiver_length_scaling = false; // Don't (auto)scale the lengths of the vectors
        vmp->quiver_length_gain = 0.5f;        // Apply a fixed gain to the length of the quivers on screen
        vmp->fixed_quiver_thickness = 0.01f/5; // Fixed quiver thickness
        vmp->finalize();
        v.addVisualModel (vmp);
    }

    v.keepOpen();

    return 0;
}
