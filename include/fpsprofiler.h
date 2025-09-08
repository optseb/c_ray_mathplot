/*
 * A profiler that computes FPS and manages an std::string that can be used on a graphical info
 * screen.
 *
 * Author: Seb James
 * Date: 2024-2025
 */
#pragma once

#include <chrono>
#include <deque>
#include <string>
#include <sstream>
#include <cmath>

namespace demo::fps
{
    using namespace std::chrono;
    using sc = std::chrono::steady_clock;

    struct profiler
    {
        sc::time_point t0 = sc::now();
        sc::time_point t1 = sc::now();

        std::deque<double> fps;
        double fps_mean = 0.0; // a running-mean of fps
        unsigned int fps_mean_over_n_samples_last = 0;

        // Current FPS text
        std::string fps_txt;

        // Call at the start of the loop that you're timing
        void at_begin (int csampl)
        {
            sc::duration t_d = this->t1 - this->t0;
            unsigned int fps_mean_over_n_samples = best_n_samples (csampl);
            if (fps_mean_over_n_samples != fps_mean_over_n_samples_last) {
                // Reset counters
                this->fps.clear();
                this->fps_mean = 0.0;
                this->fps_mean_over_n_samples_last = fps_mean_over_n_samples;
            }
            double fps_mean_period = 1.0 / fps_mean_over_n_samples;
            double fps_now = 0.0;
            double usecs = static_cast<double>(duration_cast<microseconds>(t_d).count());
            if (usecs > 0.0) { fps_now = 1000000.0 / usecs; }
            this->fps.push_back (fps_now * fps_mean_period);
            this->fps_mean += this->fps.back();
            if (this->fps.size() > fps_mean_over_n_samples) {
                this->fps_mean -= this->fps.front();
                this->fps.pop_front();
            }
            std::stringstream ss;
            // Stream into ss ready for display
            ss << csampl << " samples " << static_cast<int>(std::round(this->fps_mean)) << " FPS";
            this->fps_txt = ss.str();

            this->t0 = sc::now();
        }

        // Call at the end of the loop that you're timing
        void at_end () { this->t1 = sc::now(); }

        // For a given samples per omm, return a sensible number of loops over which to
        // average fps, so that fps takes around 1 sec to stabilize.
        static constexpr unsigned int best_n_samples (int samples_per_omm)
        {
            unsigned int best_n = 0;
            switch (samples_per_omm) {
            case 1:
            case 2:
            {
                best_n = 1024; // about a seconds worth
                break;
            }
            case 4:
            case 8:
            case 16:
            case 32:
            case 64:
            {
                best_n = 512;
                break;
            }
            case 128:
            case 256:
            {
                best_n = 256;
                break;
            }
            case 512:
            {
                best_n = 128;
                break;
            }
            case 1024:
            case 2048:
            {
                best_n = 64;
                break;
            }
            default:
            {
                best_n = 32;
            }
            }
            return best_n;
        }
    };

} // namespace
