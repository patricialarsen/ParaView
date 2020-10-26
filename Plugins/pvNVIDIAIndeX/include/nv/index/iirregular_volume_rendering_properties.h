/******************************************************************************
 * Copyright 2020 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/
/**
   \file
   \brief        Scene attribute controlling irregular volume rendering.
*/


#ifndef NVIDIA_INDEX_IIRREGULAR_VOLUME_RENDERING_PROPERTIES_H
#define NVIDIA_INDEX_IIRREGULAR_VOLUME_RENDERING_PROPERTIES_H

#include <mi/dice.h>
#include <nv/index/iattribute.h>


namespace nv {
namespace index {

/// Interface representing rendering properties for irregular volumes.
///
/// \ingroup nv_index_scene_description_attribute
///
class IIrregular_volume_rendering_properties :
    public mi::base::Interface_declare<0x72327639,0xd6ed,0x4fc9,0xba,0x2f,0x92,0xf3,0x94,0x2a,0xee,0x7c,
                                       nv::index::IAttribute>
{
public:
    /// Get default subregion halo size, in object space.
    /// Subregions are expanded by the halo size for data loading.
    /// Similar to subcube_border_size, it enables filtering and it defines the step size limit 
    /// for discrete volume sampling.
    virtual mi::Float32 get_halo_size() const = 0;
    virtual void        set_halo_size(mi::Float32 f) = 0;

    /// \name Render sampling settings
    ///@{
    /// Get render sampling mode (0 = preintegrated colormap; 1 = discrete sampling)
    virtual mi::Uint32  get_sampling_mode() const = 0;
    virtual void        set_sampling_mode(mi::Uint32 m) = 0;

    /// Get length of discrete sampling segment length on ray.
    /// Should be less than subregion halo size to avoid artifacts.
    virtual mi::Float32 get_sampling_segment_length() const = 0;
    virtual void        set_sampling_segment_length(mi::Float32 l) = 0;
        
    /// Get reference length of discrete sampling segment length on ray.
    virtual mi::Float32 get_sampling_reference_segment_length() const = 0;
    virtual void        set_sampling_reference_segment_length(mi::Float32 l) = 0;
    ///@}


    /// \name Diagnostic rendering settings
    ///@{
    /// Get diagnostic rendering mode.
    /// If not 0, a diagnostic rendering is performed instead of normal rendering (1:wireframe, 2:run path)
    virtual mi::Uint32  get_diagnostics_mode() const = 0;
    virtual void        set_diagnostics_mode(mi::Uint32 m) = 0;

    /// Get bit flags to enable various diagnostics (internal).
    virtual mi::Uint32  get_diagnostics_flags() const = 0;
    virtual void        set_diagnostics_flags(mi::Uint32 f) = 0;

    /// Get world space size of wireframe mode lines.
    virtual mi::Float32 get_wireframe_size() const = 0;
    virtual void        set_wireframe_size(mi::Float32 s) = 0;

    /// Get distance from camera where color modulation starts.
    virtual mi::Float32 get_wireframe_color_mod_begin() const = 0;
    virtual void        set_wireframe_color_mod_begin(mi::Float32 f) = 0;

    /// Get distance factor. Distance is multiplied by factor for color modulation. 0 disables color modulation.
    virtual mi::Float32 get_wireframe_color_mod_factor() const = 0;
    virtual void        set_wireframe_color_mod_factor(mi::Float32 f) = 0;
    ///@}

    
};

} // namespace index
} // namespace nv

#endif
