/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-gate
 * Created on: 3 авг. 2021 г.
 *
 * lsp-plugins-gate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-gate is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-gate. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PRIVATE_META_GATE_H_
#define PRIVATE_META_GATE_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>


namespace lsp
{
    namespace meta
    {
        struct gate_metadata
        {
            static constexpr float  THRESHOLD_MIN           = GAIN_AMP_M_60_DB;
            static constexpr float  THRESHOLD_MAX           = GAIN_AMP_0_DB;
            static constexpr float  THRESHOLD_DFL           = GAIN_AMP_M_24_DB;
            static constexpr float  THRESHOLD_STEP          = 0.05f;

            static constexpr float  H_THRESHOLD_MIN         = GAIN_AMP_M_60_DB;
            static constexpr float  H_THRESHOLD_MAX         = GAIN_AMP_0_DB;
            static constexpr float  H_THRESHOLD_DFL         = GAIN_AMP_M_12_DB;
            static constexpr float  H_THRESHOLD_STEP        = 0.05f;

            static constexpr float  REDUCTION_MIN           = GAIN_AMP_M_72_DB;
            static constexpr float  REDUCTION_MAX           = GAIN_AMP_0_DB;
            static constexpr float  REDUCTION_DFL           = GAIN_AMP_M_24_DB;
            static constexpr float  REDUCTION_STEP          = 0.05f;

            static constexpr float  ATTACK_TIME_MIN         = 0.0f;
            static constexpr float  ATTACK_TIME_MAX         = 2000.0f;
            static constexpr float  ATTACK_TIME_DFL         = 20.0f;
            static constexpr float  ATTACK_TIME_STEP        = 0.0025f;

            static constexpr float  RELEASE_TIME_MIN        = 0.0f;
            static constexpr float  RELEASE_TIME_MAX        = 5000.0f;
            static constexpr float  RELEASE_TIME_DFL        = 100.0f;
            static constexpr float  RELEASE_TIME_STEP       = 0.0025f;

            static constexpr float  MAKEUP_MIN              = GAIN_AMP_M_60_DB;
            static constexpr float  MAKEUP_MAX              = GAIN_AMP_P_60_DB;
            static constexpr float  MAKEUP_DFL              = GAIN_AMP_0_DB;
            static constexpr float  MAKEUP_STEP             = 0.05f;

            static constexpr float  ZONE_MIN                = GAIN_AMP_M_60_DB;
            static constexpr float  ZONE_MAX                = GAIN_AMP_0_DB;
            static constexpr float  ZONE_DFL                = GAIN_AMP_M_6_DB;
            static constexpr float  ZONE_STEP               = 0.05f;

            static constexpr float  LOOKAHEAD_MIN           = 0.0f;
            static constexpr float  LOOKAHEAD_MAX           = 20.0f;
            static constexpr float  LOOKAHEAD_DFL           = 0.0f;
            static constexpr float  LOOKAHEAD_STEP          = 0.01f;

            static constexpr float  REACTIVITY_MIN          = 0.000;    // Minimum reactivity [ms]
            static constexpr float  REACTIVITY_MAX          = 250;      // Maximum reactivity [ms]
            static constexpr float  REACTIVITY_DFL          = 10;       // Default reactivity [ms]
            static constexpr float  REACTIVITY_STEP         = 0.01;     // Reactivity step

            static constexpr size_t SC_MODE_DFL             = 1;
            static constexpr size_t SC_SOURCE_DFL           = 0;
            static constexpr size_t SC_TYPE_DFL             = 0;

            static constexpr float  HPF_MIN                 = 10.0f;
            static constexpr float  HPF_MAX                 = 20000.0f;
            static constexpr float  HPF_DFL                 = 10.0f;
            static constexpr float  HPF_STEP                = 0.0025f;

            static constexpr float  LPF_MIN                 = 10.0f;
            static constexpr float  LPF_MAX                 = 20000.0f;
            static constexpr float  LPF_DFL                 = 20000.0f;
            static constexpr float  LPF_STEP                = 0.0025f;

            static constexpr size_t CURVE_MESH_SIZE         = 256;
            static constexpr float  CURVE_DB_MIN            = -72;
            static constexpr float  CURVE_DB_MAX            = +24;

            static constexpr size_t TIME_MESH_SIZE          = 400;
            static constexpr float  TIME_HISTORY_MAX        = 5.0f;

            enum mode_t
            {
                EM_DOWNWARD,
                EM_UPWARD
            };

            static constexpr size_t EM_DEFAULT              = EM_UPWARD;
        };

        extern const meta::plugin_t gate_mono;
        extern const meta::plugin_t gate_stereo;
        extern const meta::plugin_t gate_lr;
        extern const meta::plugin_t gate_ms;
        extern const meta::plugin_t sc_gate_mono;
        extern const meta::plugin_t sc_gate_stereo;
        extern const meta::plugin_t sc_gate_lr;
        extern const meta::plugin_t sc_gate_ms;
    } // namespace meta
} // namespace lsp


#endif /* PRIVATE_META_GATE_H_ */
