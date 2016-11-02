#ifndef slic3r_Layer_hpp_
#define slic3r_Layer_hpp_

#include "libslic3r.h"
#include "Flow.hpp"
#include "SurfaceCollection.hpp"
#include "ExtrusionEntityCollection.hpp"
#include "ExPolygonCollection.hpp"
#include "PolylineCollection.hpp"


namespace Slic3r {

typedef std::pair<coordf_t,coordf_t> t_layer_height_range;
typedef std::map<t_layer_height_range,coordf_t> t_layer_height_ranges;

class Layer;
class PrintRegion;
class PrintObject;


// TODO: make stuff private
class LayerRegion
{
    friend class Layer;

    public:
    Layer* layer();
    PrintRegion* region() { return this->_region; }
    const PrintRegion* region() const { return this->_region; }

    // collection of surfaces generated by slicing the original geometry
    // divided by type top/bottom/internal
    SurfaceCollection slices;

    // collection of extrusion paths/loops filling gaps
    ExtrusionEntityCollection thin_fills;

    // collection of surfaces for infill generation
    SurfaceCollection fill_surfaces;

    // Collection of perimeter surfaces. This is a cached result of diff(slices, fill_surfaces).
    // While not necessary, the memory consumption is meager and it speeds up calculation.
    // The perimeter_surfaces keep the IDs of the slices (top/bottom/)
    SurfaceCollection perimeter_surfaces;

    // collection of expolygons representing the bridged areas (thus not
    // needing support material)
    Polygons bridged;

    // collection of polylines representing the unsupported bridge edges
    PolylineCollection unsupported_bridge_edges;

    // ordered collection of extrusion paths/loops to build all perimeters
    // (this collection contains only ExtrusionEntityCollection objects)
    ExtrusionEntityCollection perimeters;

    // ordered collection of extrusion paths to fill surfaces
    // (this collection contains only ExtrusionEntityCollection objects)
    ExtrusionEntityCollection fills;
    
    Flow flow(FlowRole role, bool bridge = false, double width = -1) const;
    void merge_slices();
    void prepare_fill_surfaces();
    void make_perimeters(const SurfaceCollection &slices, SurfaceCollection* perimeter_surfaces, SurfaceCollection* fill_surfaces);
    void process_external_surfaces(const Layer* lower_layer);
    double infill_area_threshold() const;

    void export_region_slices_to_svg(const char *path);
    void export_region_fill_surfaces_to_svg(const char *path);
    // Export to "out/LayerRegion-name-%d.svg" with an increasing index with every export.
    void export_region_slices_to_svg_debug(const char *name);
    void export_region_fill_surfaces_to_svg_debug(const char *name);

    private:
    Layer *_layer;
    PrintRegion *_region;

    LayerRegion(Layer *layer, PrintRegion *region);
    ~LayerRegion();
};


typedef std::vector<LayerRegion*> LayerRegionPtrs;

class Layer {
    friend class PrintObject;

public:
    size_t id() const;
    void set_id(size_t id);
    PrintObject* object();
    const PrintObject* object() const;

    Layer *upper_layer;
    Layer *lower_layer;
    LayerRegionPtrs regions;
    bool slicing_errors;
    coordf_t slice_z;       // Z used for slicing in unscaled coordinates
    coordf_t print_z;       // Z used for printing in unscaled coordinates
    coordf_t height;        // layer height in unscaled coordinates

    // collection of expolygons generated by slicing the original geometry;
    // also known as 'islands' (all regions and surface types are merged here)
    ExPolygonCollection slices;
    // Surfaces of the perimeters including their gap fill.
    ExPolygonCollection perimeter_expolygons;


    size_t region_count() const;
    LayerRegion* get_region(int idx);
    LayerRegion* add_region(PrintRegion* print_region);
    
    void make_slices();
    void merge_slices();
    template <class T> bool any_internal_region_slice_contains(const T &item) const;
    template <class T> bool any_bottom_region_slice_contains(const T &item) const;
    void make_perimeters();
    void make_fills();

    void export_region_slices_to_svg(const char *path);
    void export_region_fill_surfaces_to_svg(const char *path);
    // Export to "out/LayerRegion-name-%d.svg" with an increasing index with every export.
    void export_region_slices_to_svg_debug(const char *name);
    void export_region_fill_surfaces_to_svg_debug(const char *name);
    
protected:
    size_t _id;     // sequential number of layer, 0-based
    PrintObject *_object;


    Layer(size_t id, PrintObject *object, coordf_t height, coordf_t print_z,
        coordf_t slice_z);
    virtual ~Layer();

    void clear_regions();
    void delete_region(int idx);
};


class SupportLayer : public Layer {
    friend class PrintObject;

public:
    // Polygons covered by the supports: base, interface and contact areas.
    ExPolygonCollection support_islands;
    // Extrusion paths for the support base.
    ExtrusionEntityCollection support_fills;
    // Extrusion paths for the support interface and contacts.
    ExtrusionEntityCollection support_interface_fills;

protected:
    SupportLayer(size_t id, PrintObject *object, coordf_t height, coordf_t print_z,
        coordf_t slice_z);
    virtual ~SupportLayer();
};


}

#endif
