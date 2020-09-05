// MIT License

// Copyright (c) 2020 Evan Pezent

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// ImPlot v0.7 WIP

// You may use this file to debug, understand or extend ImPlot features but we
// don't provide any guarantee of forward compatibility!

//-----------------------------------------------------------------------------
// [SECTION] Header Mess
//-----------------------------------------------------------------------------

#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <time.h>
#include "imgui_internal.h"

#ifndef IMPLOT_VERSION
#error Must include implot.h before implot_internal.h
#endif

//-----------------------------------------------------------------------------
// [SECTION] Forward Declarations
//-----------------------------------------------------------------------------

struct ImPlotTick;
struct ImPlotAxis;
struct ImPlotAxisState;
struct ImPlotAxisColor;
struct ImPlotItem;
struct ImPlotState;
struct ImPlotNextPlotData;

//-----------------------------------------------------------------------------
// [SECTION] Context Pointer
//-----------------------------------------------------------------------------

extern ImPlotContext* GImPlot; // Current implicit context pointer

//-----------------------------------------------------------------------------
// [SECTION] Macros and Constants
//-----------------------------------------------------------------------------

// Constants can be changed unless stated otherwise. We may move some of these
// to ImPlotStyleVar_ over time.

// Default plot frame width when requested width is auto (i.e. 0). This is not the plot area width!
#define IMPLOT_DEFAULT_W  400
// Default plot frame height when requested height is auto (i.e. 0). This is not the plot area height!
#define IMPLOT_DEFAULT_H  300
// The maximum number of supported y-axes (DO NOT CHANGE THIS)
#define IMPLOT_Y_AXES     3
// The number of times to subdivided grid divisions (best if a multiple of 1, 2, and 5)
#define IMPLOT_SUB_DIV    10
// Zoom rate for scroll (e.g. 0.1f = 10% plot range every scroll click)
#define IMPLOT_ZOOM_RATE  0.1f
// Maximum allowable timestamp value 01/01/3000 @ 12:00am (UTC)
#define IMPLOT_MIN_TIME 0
// Maximum allowable timestamp value 01/01/3000 @ 12:00am (UTC)
#define IMPLOT_MAX_TIME 32503680000

//-----------------------------------------------------------------------------
// [SECTION] Generic Helpers
//-----------------------------------------------------------------------------

// Computes the common (base-10) logarithm
static inline float  ImLog10(float x)  { return log10f(x); }
static inline double ImLog10(double x) { return log10(x);  }
// Returns true if a flag is set
template <typename TSet, typename TFlag>
inline bool ImHasFlag(TSet set, TFlag flag) { return (set & flag) == flag; }
// Flips a flag in a flagset
template <typename TSet, typename TFlag>
inline void ImFlipFlag(TSet& set, TFlag flag) { ImHasFlag(set, flag) ? set &= ~flag : set |= flag; }
// Linearly remaps x from [x0 x1] to [y0 y1].
template <typename T>
inline T ImRemap(T x, T x0, T x1, T y0, T y1) { return y0 + (x - x0) * (y1 - y0) / (x1 - x0); }
// Returns always positive modulo (assumes r != 0)
inline int ImPosMod(int l, int r) { return (l % r + r) % r; }
// Returns true if val is NAN or INFINITY
inline bool ImNanOrInf(double val) { return val == HUGE_VAL || val == -HUGE_VAL || isnan(val); }
// Turns NANs to 0s
inline double ImConstrainNan(double val) { return isnan(val) ? 0 : val; }
// Turns infinity to floating point maximums
inline double ImConstrainInf(double val) { return val == HUGE_VAL ?  DBL_MAX : val == -HUGE_VAL ? - DBL_MAX : val; }
// Turns numbers less than or equal to 0 to 0.001 (sort of arbitrary, is there a better way?)
inline double ImConstrainLog(double val) { return val <= 0 ? 0.001f : val; }
// Turns numbers less than 0 to zero
inline double ImConstrainTime(double val) { return val < IMPLOT_MIN_TIME ? IMPLOT_MIN_TIME : (val > IMPLOT_MAX_TIME ? IMPLOT_MAX_TIME : val); }

// Offset calculator helper
template <int Count>
struct ImOffsetCalculator {
    ImOffsetCalculator(const int* sizes) {
        Offsets[0] = 0;
        for (int i = 1; i < Count; ++i)
            Offsets[i] = Offsets[i-1] + sizes[i-1];
    }
    int Offsets[Count];
};

// Character buffer writer helper
struct ImBufferWriter
{
    char*  Buffer;
    size_t Size;
    size_t Pos;

    ImBufferWriter(char* buffer, size_t size) {
        Buffer = buffer;
        Size = size;
        Pos = 0;
    }

    void Write(const char* fmt, ...) IM_FMTARGS(2) {
        va_list argp;
        va_start(argp, fmt);
        const int written = ::vsnprintf(&Buffer[Pos], Size - Pos - 1, fmt, argp);
        if (written > 0)
          Pos += ImMin(size_t(written), Size-Pos-1);
        va_end(argp);
    }
};

// Fixed size point array
template <int N>
struct ImPlotPointArray {
    inline ImPlotPoint&       operator[](int i)       { return Data[i]; }
    inline const ImPlotPoint& operator[](int i) const { return Data[i]; }
    ImPlotPoint Data[N];
    const int Size = N;
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot Enums
//-----------------------------------------------------------------------------

typedef int ImPlotScale;     // -> enum ImPlotScale_
typedef int ImPlotTimeUnit;  // -> enum ImPlotTimeUnit_
typedef int ImPlotTimeFmt;   // -> enum ImPlotTimeFmt_

// XY axes scaling combinations
enum ImPlotScale_ {
    ImPlotScale_LinLin, // linear x, linear y
    ImPlotScale_LogLin, // log x,    linear y
    ImPlotScale_LinLog, // linear x, log y
    ImPlotScale_LogLog  // log x,    log y
};

//-----------------------------------------------------------------------------
// [SECTION] ImPlot Structs
//-----------------------------------------------------------------------------

// Storage for colormap modifiers
struct ImPlotColormapMod {
    ImPlotColormapMod(const ImVec4* colormap, int colormap_size) {
        Colormap     = colormap;
        ColormapSize = colormap_size;
    }
    const ImVec4* Colormap;
    int ColormapSize;
};

// ImPlotPoint with positive/negative error values
struct ImPlotPointError
{
    double X, Y, Neg, Pos;

    ImPlotPointError(double x, double y, double neg, double pos) {
        X = x; Y = y; Neg = neg; Pos = pos;
    }
};

// Tick mark info
struct ImPlotTick
{
    double PlotPos;
    float  PixelPos;
    ImVec2 LabelSize;
    int    BufferOffset;
    bool   Major;
    bool   ShowLabel;
    int    Level;

    ImPlotTick(double value, bool major, bool show_label) {
        PlotPos      = value;
        Major        = major;
        ShowLabel    = show_label;
        BufferOffset = -1;
        Level        = 0;
    }
};

// Collection of ticks
struct ImPlotTickCollection {
    ImVector<ImPlotTick> Ticks;
    ImGuiTextBuffer      Labels;
    float                TotalWidth;
    float                TotalHeight;
    float                MaxWidth;
    float                MaxHeight;
    int                  Size;

    void AddTick(const ImPlotTick& tick) {
        if (tick.ShowLabel) {
            TotalWidth  += tick.ShowLabel ? tick.LabelSize.x : 0;
            TotalHeight += tick.ShowLabel ? tick.LabelSize.y : 0;
            MaxWidth    =  tick.LabelSize.x > MaxWidth  ? tick.LabelSize.x : MaxWidth;
            MaxHeight   =  tick.LabelSize.y > MaxHeight ? tick.LabelSize.y : MaxHeight;
        }
        Ticks.push_back(tick);
        Size++;
    }

    void AddTick(double value, bool major, bool show_label, void (*labeler)(ImPlotTick& tick, ImGuiTextBuffer& buf)) {
        ImPlotTick tick(value, major, show_label);
        if (labeler)
            labeler(tick, Labels);
        AddTick(tick);
    }

    const char* GetLabel(int idx) {
        return Labels.Buf.Data + Ticks[idx].BufferOffset;
    }

    void Reset() {
        Ticks.shrink(0);
        Labels.Buf.shrink(0);
        TotalWidth = TotalHeight = MaxWidth = MaxHeight = 0;
        Size = 0;
    }

};

// Axis state information that must persist after EndPlot
struct ImPlotAxis
{
    ImPlotAxisFlags Flags;
    ImPlotAxisFlags PreviousFlags;
    ImPlotRange     Range;
    bool            Dragging;
    bool            HoveredExt;
    bool            HoveredTot;

    ImPlotAxis() {
        Flags      = PreviousFlags = ImPlotAxisFlags_Default;
        Range.Min  = 0;
        Range.Max  = 1;
        Dragging   = false;
        HoveredExt = false;
        HoveredTot = false;
    }

    bool SetMin(double _min) { 
        _min = ImConstrainNan(ImConstrainInf(_min));
        if (ImHasFlag(Flags, ImPlotAxisFlags_LogScale))
            _min = ImConstrainLog(_min);
        if (ImHasFlag(Flags, ImPlotAxisFlags_Time)) { 
            _min = ImConstrainTime(_min);  
            if ((Range.Max - _min) < 0.0001)
                return false;
        }          
        if (_min >= Range.Max) 
            return false;
        Range.Min = _min;
        return true;        
    };

    bool SetMax(double _max) { 
        _max = ImConstrainNan(ImConstrainInf(_max));
        if (ImHasFlag(Flags, ImPlotAxisFlags_LogScale))
            _max = ImConstrainLog(_max);
        if (ImHasFlag(Flags, ImPlotAxisFlags_Time)) { 
            _max = ImConstrainTime(_max);  
            if ((_max - Range.Min) < 0.0001)
                return false;
        }          
        if (_max <= Range.Min) 
            return false;
        Range.Max = _max;
        return true;  
    };

    void Constrain() {
        Range.Min = ImConstrainNan(ImConstrainInf(Range.Min));
        Range.Max = ImConstrainNan(ImConstrainInf(Range.Max));
        if (ImHasFlag(Flags, ImPlotAxisFlags_LogScale)) {
            Range.Min = ImConstrainLog(Range.Min);
            Range.Max = ImConstrainLog(Range.Max);
        }
        if (ImHasFlag(Flags, ImPlotAxisFlags_Time)) {
            Range.Min = ImConstrainTime(Range.Min);
            Range.Max = ImConstrainTime(Range.Max);
            if (Range.Size() < 0.0001)
                Range.Max = Range.Min + 0.0001; // TBD
        }
        if (Range.Max <= Range.Min)
            Range.Max = Range.Min + DBL_EPSILON;
    }
};

// Axis state information only needed between BeginPlot/EndPlot
struct ImPlotAxisState
{
    ImPlotAxis* Axis;
    ImGuiCond   RangeCond;
    bool        HasRange;
    bool        Present;
    bool        HasLabels;
    bool        Invert;
    bool        LockMin;
    bool        LockMax;
    bool        Lock;
    bool        IsTime;

    ImPlotAxisState(ImPlotAxis* axis, bool has_range, ImGuiCond range_cond, bool present) {
        Axis         = axis;
        HasRange     = has_range;
        RangeCond    = range_cond;
        Present      = present;
        HasLabels    = ImHasFlag(Axis->Flags, ImPlotAxisFlags_TickLabels);
        Invert       = ImHasFlag(Axis->Flags, ImPlotAxisFlags_Invert);
        LockMin      = ImHasFlag(Axis->Flags, ImPlotAxisFlags_LockMin) || (HasRange && RangeCond == ImGuiCond_Always);
        LockMax      = ImHasFlag(Axis->Flags, ImPlotAxisFlags_LockMax) || (HasRange && RangeCond == ImGuiCond_Always);
        Lock         = !Present || ((LockMin && LockMax) || (HasRange && RangeCond == ImGuiCond_Always));
        IsTime       = ImHasFlag(Axis->Flags, ImPlotAxisFlags_Time);
    }

    ImPlotAxisState() { }
};

struct ImPlotAxisColor
{
    ImU32 Major, Minor, MajTxt, MinTxt;
    ImPlotAxisColor() { Major = Minor = MajTxt = MinTxt = 0; }
};

// State information for Plot items
struct ImPlotItem
{
    ImGuiID      ID;
    ImVec4       Color;
    int          NameOffset;
    bool         Show;
    bool         LegendHovered;
    bool         SeenThisFrame;

    ImPlotItem() {
        ID            = 0;
        Color         = ImPlot::NextColormapColor();
        NameOffset    = -1;
        Show          = true;
        SeenThisFrame = false;
        LegendHovered = false;
    }

    ~ImPlotItem() { ID = 0; }
};

// Holds Plot state information that must persist after EndPlot
struct ImPlotState
{
    ImPlotFlags        Flags;
    ImPlotFlags        PreviousFlags;
    ImPlotAxis         XAxis;
    ImPlotAxis         YAxis[IMPLOT_Y_AXES];
    ImPool<ImPlotItem> Items;
    ImVec2             SelectStart;
    ImVec2             QueryStart;
    ImRect             QueryRect;
    ImRect             BB_Legend;
    bool               Selecting;
    bool               Querying;
    bool               Queried;
    bool               DraggingQuery;
    int                ColormapIdx;
    int                CurrentYAxis;

    ImPlotState() {
        Flags        = PreviousFlags = ImPlotFlags_Default;
        SelectStart  = QueryStart = ImVec2(0,0);
        Selecting    = Querying = Queried = DraggingQuery = false;
        ColormapIdx  = CurrentYAxis = 0;
    }
};

// Temporary data storage for upcoming plot
struct ImPlotNextPlotData
{
    ImGuiCond   XRangeCond;
    ImGuiCond   YRangeCond[IMPLOT_Y_AXES];
    ImPlotRange X;
    ImPlotRange Y[IMPLOT_Y_AXES];
    bool        HasXRange;
    bool        HasYRange[IMPLOT_Y_AXES];
    bool        ShowDefaultTicksX;
    bool        ShowDefaultTicksY[IMPLOT_Y_AXES];
    bool        FitX;
    bool        FitY[IMPLOT_Y_AXES];

    ImPlotNextPlotData() {
        HasXRange         = false;
        ShowDefaultTicksX = true;
        FitX              = false;
        for (int i = 0; i < IMPLOT_Y_AXES; ++i) {
            HasYRange[i]         = false;
            ShowDefaultTicksY[i] = true;
            FitY[i]              = false;
        }
    }
};

// Temporary data storage for upcoming item
struct ImPlotItemStyle {
    ImVec4       Colors[5]; // ImPlotCol_Line, ImPlotCol_Fill, ImPlotCol_MarkerOutline, ImPlotCol_MarkerFill, ImPlotCol_ErrorBar
    float        LineWeight;
    ImPlotMarker Marker;
    float        MarkerSize;
    float        MarkerWeight;
    float        FillAlpha;
    float        ErrorBarSize;
    float        ErrorBarWeight;
    float        DigitalBitHeight;
    float        DigitalBitGap;
    bool         RenderLine;
    bool         RenderFill;
    bool         RenderMarkerLine;
    bool         RenderMarkerFill;
    ImPlotItemStyle() {
        for (int i = 0; i < 5; ++i)
            Colors[i] = IMPLOT_AUTO_COL;
        LineWeight = MarkerSize = MarkerWeight = FillAlpha = ErrorBarSize =
        ErrorBarSize = ErrorBarWeight = DigitalBitHeight = DigitalBitGap = IMPLOT_AUTO;
        Marker = IMPLOT_AUTO;
    }
};

// Holds state information that must persist between calls to BeginPlot()/EndPlot()
struct ImPlotContext {
    // Plot States
    ImPool<ImPlotState> Plots;
    ImPlotState*        CurrentPlot;
    ImPlotItem*         CurrentItem;

    // Legend
    ImVector<int>   LegendIndices;
    ImGuiTextBuffer LegendLabels;

    // Bounding Boxes
    ImRect BB_Frame;
    ImRect BB_Canvas;
    ImRect BB_Plot;

    // Axis States
    ImPlotAxisColor Col_X;
    ImPlotAxisColor Col_Y[IMPLOT_Y_AXES];
    ImPlotAxisState X;
    ImPlotAxisState Y[IMPLOT_Y_AXES];

    // Tick Marks and Labels
    ImPlotTickCollection XTicks;
    ImPlotTickCollection YTicks[IMPLOT_Y_AXES];
    float                YAxisReference[IMPLOT_Y_AXES];

    // Transformations and Data Extents
    ImPlotScale Scales[IMPLOT_Y_AXES];
    ImRect      PixelRange[IMPLOT_Y_AXES];
    double      Mx;
    double      My[IMPLOT_Y_AXES];
    double      LogDenX;
    double      LogDenY[IMPLOT_Y_AXES];
    ImPlotRange ExtentsX;
    ImPlotRange ExtentsY[IMPLOT_Y_AXES];

    // Data Fitting Flags
    bool FitThisFrame;
    bool FitX;
    bool FitY[IMPLOT_Y_AXES];

    // Hover states
    bool Hov_Frame;
    bool Hov_Plot;

    // Axis Rendering Flags
    bool RenderX;
    bool RenderY[IMPLOT_Y_AXES];

    // Axis Locking Flags
    bool LockPlot;
    bool ChildWindowMade;

    // Style and Colormaps
    ImPlotStyle                 Style;
    ImVector<ImGuiColorMod>     ColorModifiers;
    ImVector<ImGuiStyleMod>     StyleModifiers;
    const ImVec4*               Colormap;
    int                         ColormapSize;
    ImVector<ImPlotColormapMod> ColormapModifiers;

    // Time
    tm Tm;

    // Misc
    int                VisibleItemCount;
    int                DigitalPlotItemCnt;
    int                DigitalPlotOffset;
    ImPlotNextPlotData NextPlotData;
    ImPlotItemStyle    NextItemStyle;
    ImPlotInputMap     InputMap;
    ImPlotPoint        MousePos[IMPLOT_Y_AXES];
};

struct ImPlotAxisScale
{
    ImPlotPoint Min, Max;

    ImPlotAxisScale(int y_axis, float tx, float ty, float zoom_rate) {
        ImPlotContext& gp = *GImPlot;
        Min = ImPlot::PixelsToPlot(gp.BB_Plot.Min - gp.BB_Plot.GetSize() * ImVec2(tx * zoom_rate, ty * zoom_rate), y_axis);
        Max = ImPlot::PixelsToPlot(gp.BB_Plot.Max + gp.BB_Plot.GetSize() * ImVec2((1 - tx) * zoom_rate, (1 - ty) * zoom_rate), y_axis);
    }
};

//-----------------------------------------------------------------------------
// [SECTION] Internal API
// No guarantee of forward compatibility here!
//-----------------------------------------------------------------------------

namespace ImPlot {

//-----------------------------------------------------------------------------
// [SECTION] Context Utils
//-----------------------------------------------------------------------------

// Initializes an ImPlotContext
void Initialize(ImPlotContext* ctx);
// Resets an ImPlot context for the next call to BeginPlot
void Reset(ImPlotContext* ctx);

//-----------------------------------------------------------------------------
// [SECTION] Plot Utils
//-----------------------------------------------------------------------------

// Gets a plot from the current ImPlotContext
ImPlotState* GetPlot(const char* title);
// Gets the current plot from the current ImPlotContext
ImPlotState* GetCurrentPlot();
// Busts the cache for every plot in the current context
void BustPlotCache();

//-----------------------------------------------------------------------------
// [SECTION] Item Utils
//-----------------------------------------------------------------------------

// Begins a new item. Returns false if the item should not be plotted. Pushes PlotClipRect.
bool BeginItem(const char* label_id, ImPlotCol recolor_from = -1);
// Ends an item (call only if BeginItem returns true). Pops PlotClipRect.
void EndItem();

// Register or get an existing item from the current plot
ImPlotItem* RegisterOrGetItem(const char* label_id);
// Get the ith plot item from the current plot
ImPlotItem* GetItem(int i);
// Get a plot item from the current plot
ImPlotItem* GetItem(const char* label_id);
// Gets a plot item from a specific plot
ImPlotItem* GetItem(const char* plot_title, const char* item_label_id);
// Gets the current item
ImPlotItem* GetCurrentItem();
// Busts the cache for every item for every plot in the current context.
void BustItemCache();

//-----------------------------------------------------------------------------
// [SECTION] Axis Utils
//-----------------------------------------------------------------------------

// Gets the current y-axis for the current plot
inline int GetCurrentYAxis() { return GImPlot->CurrentPlot->CurrentYAxis; }
// Updates axis ticks, lins, and label colors
void UpdateAxisColors(int axis_flag, ImPlotAxisColor* col);

// Updates plot-to-pixel space transformation variables for the current plot.
void UpdateTransformCache();
// Gets the XY scale for the current plot and y-axis
inline ImPlotScale GetCurrentScale() { return GImPlot->Scales[GetCurrentYAxis()]; }

// Returns true if the user has requested data to be fit.
inline bool FitThisFrame() { return GImPlot->FitThisFrame; }
// Extends the current plots axes so that it encompasses point p
void FitPoint(const ImPlotPoint& p);

//-----------------------------------------------------------------------------
// [SECTION] Legend Utils
//-----------------------------------------------------------------------------

// Returns the number of entries in the current legend
int GetLegendCount();
// Gets the ith entry string for the current legend
const char* GetLegendLabel(int i);

//-----------------------------------------------------------------------------
// [SECTION] Tick Utils
//-----------------------------------------------------------------------------

// Label a tick with default formatting.
void LabelTickDefault(ImPlotTick& tick, ImGuiTextBuffer& buffer);
// Label a tick with scientific formating.
void LabelTickScientific(ImPlotTick& tick, ImGuiTextBuffer& buffer);
// Label a tick with time formatting.
void LabelTickTime(ImPlotTick& tick, ImGuiTextBuffer& buffer, ImPlotTimeFmt fmt);

// Populates a list of ImPlotTicks with normal spaced and formatted ticks
void AddTicksDefault(const ImPlotRange& range, int nMajor, int nMinor, ImPlotTickCollection& ticks);
// Populates a list of ImPlotTicks with logarithmic space and formatted ticks
void AddTicksLogarithmic(const ImPlotRange& range, int nMajor, ImPlotTickCollection& ticks);
// Populates a list of ImPlotTicks with time formatted ticks.
void AddTicksTime(const ImPlotRange& range, int nMajor, ImPlotTickCollection& ticks);
// Populates a list of ImPlotTicks with custom spaced and labeled ticks
void AddTicksCustom(const double* values, const char** labels, int n, ImPlotTickCollection& ticks);

//-----------------------------------------------------------------------------
// [SECTION] Styling Utils
//-----------------------------------------------------------------------------

// Get styling data for next item (call between Begin/EndItem)
inline const ImPlotItemStyle& GetItemStyle() { return GImPlot->NextItemStyle; }

// Returns true if a color is set to be automatically determined
inline bool IsColorAuto(const ImVec4& col) { return col.w == -1; }
// Returns true if a style color is set to be automaticaly determined
inline bool IsColorAuto(ImPlotCol idx) { return IsColorAuto(GImPlot->Style.Colors[idx]); }
// Returns the automatically deduced style color
ImVec4 GetAutoColor(ImPlotCol idx);

// Returns the style color whether it is automatic or custom set
inline ImVec4 GetStyleColorVec4(ImPlotCol idx) { return IsColorAuto(idx) ? GetAutoColor(idx) : GImPlot->Style.Colors[idx]; }
inline ImU32  GetStyleColorU32(ImPlotCol idx)  { return ImGui::ColorConvertFloat4ToU32(GetStyleColorVec4(idx)); }

// Get built-in colormap data and size
const ImVec4* GetColormap(ImPlotColormap colormap, int* size_out);
// Linearly interpolates a color from the current colormap given t between 0 and 1.
ImVec4 LerpColormap(const ImVec4* colormap, int size, float t);
// Resamples a colormap. #size_out must be greater than 1.
void ResampleColormap(const ImVec4* colormap_in, int size_in, ImVec4* colormap_out, int size_out);

// Draws vertical text. The position is the bottom left of the text rect.
void AddTextVertical(ImDrawList *DrawList, ImVec2 pos, ImU32 col, const char* text_begin, const char* text_end = NULL);
// Calculates the size of vertical text
inline ImVec2 CalcTextSizeVertical(const char *text) { ImVec2 sz = ImGui::CalcTextSize(text); return ImVec2(sz.y, sz.x); }
// Returns white or black text given background color
inline ImU32 CalcTextColor(const ImVec4& bg) { return (bg.x * 0.299 + bg.y * 0.587 + bg.z * 0.114) > 0.729 ? IM_COL32_BLACK : IM_COL32_WHITE; }

//-----------------------------------------------------------------------------
// [SECTION] Math and Misc Utils
//-----------------------------------------------------------------------------

// Rounds x to powers of 2,5 and 10 for generating axis labels (from Graphics Gems 1 Chapter 11.2)
double NiceNum(double x, bool round);
// Computes order of magnitude of double.
inline int OrderOfMagnitude(double val) { return val == 0 ? 0 : (int)(floor(log10(fabs(val)))); }
// Returns the precision required for a order of magnitude.
inline int OrderToPrecision(int order) { return order > 0 ? 0 : 1 - order; }
// Returns a floating point precision to use given a value
inline int Precision(double val) { return OrderToPrecision(OrderOfMagnitude(val)); }

// Returns the intersection point of two lines A and B (assumes they are not parallel!)
inline ImVec2 Intersection(const ImVec2& a1, const ImVec2& a2, const ImVec2& b1, const ImVec2& b2) {
    float v1 = (a1.x * a2.y - a1.y * a2.x);  float v2 = (b1.x * b2.y - b1.y * b2.x);
    float v3 = ((a1.x - a2.x) * (b1.y - b2.y) - (a1.y - a2.y) * (b1.x - b2.x));
    return ImVec2((v1 * (b1.x - b2.x) - v2 * (a1.x - a2.x)) / v3, (v1 * (b1.y - b2.y) - v2 * (a1.y - a2.y)) / v3);
}

// Fills a buffer with n samples linear interpolated from vmin to vmax
template <typename T>
void FillRange(ImVector<T>& buffer, int n, T vmin, T vmax) {
    buffer.resize(n);
    T step = (vmax - vmin) / (n - 1);
    for (int i = 0; i < n; ++i) {
        buffer[i] = vmin + i * step;
    }
}

// Offsets and strides a data buffer
template <typename T>
inline T OffsetAndStride(const T* data, int idx, int count, int offset, int stride) {
    idx = ImPosMod(offset + idx, count);
    return *(const T*)(const void*)((const unsigned char*)data + (size_t)idx * stride);
}

//-----------------------------------------------------------------------------
// [SECTION] Time Utils
//-----------------------------------------------------------------------------

enum ImPlotTimeUnit_ {  //                primary
    ImPlotTimeUnit_Us,  // microsecond    :29.428552
    ImPlotTimeUnit_Ms,  // millisecond    :29.428
    ImPlotTimeUnit_S,   // second         :29
    ImPlotTimeUnit_Min, // minute         7:21pm
    ImPlotTimeUnit_Hr,  // hour           7pm
    ImPlotTimeUnit_Day, // day            10/3
    ImPlotTimeUnit_Mo,  // month          Oct
    ImPlotTimeUnit_Yr,  // year           1991
    ImPlotTimeUnit_COUNT
};

enum ImPlotTimeFmt_ {
    ImPlotTimeFmt_SUs,          // :29.428552
    ImPlotTimeFmt_SMs,          // :29.428
    ImPlotTimeFmt_S,            // :29
    ImPlotTimeFmt_HrMin,        // 7:21pm
    ImPlotTimeFmt_Hr,           // 7pm
    ImPlotTimeFmt_DayMo,        // 10/3
    ImPlotTimeFmt_DayMoHrMin,   // 10/3 7:21pm
    ImPlotTimeFmt_DayMoYrHrMin, // 10/3/1991 7:21pm
    ImPlotTimeFmt_Mo,           // Oct
    ImPlotTimeFmt_Yr            // 1991
};

static const int    DaysInMonth[12]                      = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const double TimeUnitSpans[ImPlotTimeUnit_COUNT]  = {0.000001, 0.001, 1, 60, 3600, 86400, 2629800, 31557600};
static const ImPlotTimeFmt TimeFormatLevel0[ImPlotTimeUnit_COUNT] =
{
    ImPlotTimeFmt_SUs,
    ImPlotTimeFmt_SMs,
    ImPlotTimeFmt_S,
    ImPlotTimeFmt_HrMin,
    ImPlotTimeFmt_Hr,
    ImPlotTimeFmt_DayMo,
    ImPlotTimeFmt_Mo,
    ImPlotTimeFmt_Yr
};
static const ImPlotTimeFmt TimeFormatLevel1[ImPlotTimeUnit_COUNT] =
{
    ImPlotTimeFmt_DayMoHrMin,
    ImPlotTimeFmt_DayMoHrMin,
    ImPlotTimeFmt_DayMoHrMin,
    ImPlotTimeFmt_DayMoHrMin,
    ImPlotTimeFmt_DayMo,
    ImPlotTimeFmt_DayMo,
    ImPlotTimeFmt_Yr,
    ImPlotTimeFmt_Yr
};


inline ImPlotTimeUnit GetUnitForRange(double range) {
    static double cutoffs[ImPlotTimeUnit_COUNT] = {0.001, 1, 60, 3600, 86400, 2629800, 31557600, IMPLOT_MAX_TIME};
    for (int i = 0; i < ImPlotTimeUnit_COUNT; ++i) {
        if (range <= cutoffs[i])
            return (ImPlotTimeUnit)i;
    }
    return ImPlotTimeUnit_Yr;
}


// Returns true if year is leap year (366 days long)
inline bool IsLeapYear(int year) {
    if (year % 4 != 0) return false;
    if (year % 400 == 0) return true;
    if (year % 100 == 0) return false;
    return true;
}

// Returns the number of days in a month, accounting for Feb. leap years.
inline int GetDaysInMonth(int year, int month) {
    return DaysInMonth[month] + (int)(month == 1 && IsLeapYear(year));
}

inline time_t MakeGmTime(const struct tm *ptm) {
    time_t secs = 0;
    int year = ptm->tm_year + 1900;
    for (int y = 1970; y < year; ++y) {
        secs += (IsLeapYear(y)? 366: 365) * 86400;
    }
    for (int m = 0; m < ptm->tm_mon; ++m) {
        secs += DaysInMonth[m] * 86400;
        if (m == 1 && IsLeapYear(year)) secs += 86400;
    }
    secs += (ptm->tm_mday - 1) * 86400;
    secs += ptm->tm_hour       * 3600;
    secs += ptm->tm_min        * 60;
    secs += ptm->tm_sec;
    return secs;
}

inline tm* GetGmTime(const time_t* time, tm* tm)
{
#ifdef _MSC_VER
  if (gmtime_s(tm, time) == 0)
    return tm;
  else
    return NULL;
#else
  return gmtime_r(time, tm);
#endif
}

inline double AddTime(double t, ImPlotTimeUnit unit, int count) {
    switch(unit) {
        case ImPlotTimeUnit_Us:  return t + count * 0.000001;
        case ImPlotTimeUnit_Ms:  return t + count * 0.001;
        case ImPlotTimeUnit_S:   return t + count;
        case ImPlotTimeUnit_Min: return t + count * 60;
        case ImPlotTimeUnit_Hr:  return t + count * 3600;
        case ImPlotTimeUnit_Day: return t + count * 86400;
        case ImPlotTimeUnit_Yr:  count *= 12; // fall-through
        case ImPlotTimeUnit_Mo:  for (int i = 0; i < count; ++i) {
                                     time_t s = (time_t)t;
                                     GetGmTime(&s, &GImPlot->Tm);
                                     int days = GetDaysInMonth(GImPlot->Tm.tm_year, GImPlot->Tm.tm_mon);
                                     t = AddTime(t, ImPlotTimeUnit_Day, days);
                                 }
                                 return t;
        default:                 return t;
    }
}

inline int GetYear(double t) {
    time_t s = (time_t)t;
    tm& Tm = GImPlot->Tm;
    IM_ASSERT(GetGmTime(&s, &Tm) != NULL);
    return Tm.tm_year + 1900;
}

inline double MakeYear(int year) {
    int yr = year - 1900;
    if (yr < 0)
        yr = 0;
    GImPlot->Tm = tm();
    GImPlot->Tm.tm_year = yr;
    GImPlot->Tm.tm_sec  = 1;
    time_t s = MakeGmTime(&GImPlot->Tm);
    if (s < 0)
        s = 0;
    return (double)s;
}

inline double FloorTime(double t, ImPlotTimeUnit unit) {
    time_t s = (time_t)t;
    GetGmTime(&s, &GImPlot->Tm);
    GImPlot->Tm.tm_isdst = -1;
    switch (unit) {
        case ImPlotTimeUnit_S:   return (double)s;
        case ImPlotTimeUnit_Ms:  return floor(t * 1000) / 1000;
        case ImPlotTimeUnit_Us:  return floor(t * 1000000) / 1000000;
        case ImPlotTimeUnit_Yr:  GImPlot->Tm.tm_mon  = 0; // fall-through
        case ImPlotTimeUnit_Mo:  GImPlot->Tm.tm_mday = 1; // fall-through
        case ImPlotTimeUnit_Day: GImPlot->Tm.tm_hour = 0; // fall-through
        case ImPlotTimeUnit_Hr:  GImPlot->Tm.tm_min  = 0; // fall-through
        case ImPlotTimeUnit_Min: GImPlot->Tm.tm_sec  = 0; break;
        default:             return t;
    }
    s = MakeGmTime(&GImPlot->Tm);
    return (double)s;
}

inline double CeilTime(double t, ImPlotTimeUnit unit) {
    return AddTime(FloorTime(t, unit), unit, 1);
}

inline void FormatTime(double t, char* buffer, int size, ImPlotTimeFmt fmt) {
    time_t s = (time_t)t;
    int ms = (int)(t * 1000 - floor(t) * 1000);
    ms = ms % 10 == 9 ? ms + 1 : ms;
    int us = (int)(t * 1000000 - floor(t) * 1000000);
    us = us % 10 == 9 ? us + 1 : us;
    tm& Tm = GImPlot->Tm;
    IM_ASSERT(GetGmTime(&s, &Tm) != NULL);
    switch(fmt) {
        case ImPlotTimeFmt_Yr:    strftime(buffer, size, "%Y",    &Tm); break;
        case ImPlotTimeFmt_Mo:    strftime(buffer, size, "%b",    &Tm); break;
        case ImPlotTimeFmt_DayMo: snprintf(buffer, size, "%d/%d", Tm.tm_mon + 1, Tm.tm_mday); break;
        case ImPlotTimeFmt_DayMoHrMin:
            if (Tm.tm_hour == 0)
                snprintf(buffer, size, "%d/%d 12:%02dam", Tm.tm_mon + 1, Tm.tm_mday, Tm.tm_min);
            else if (Tm.tm_hour == 12)
                snprintf(buffer, size, "%d/%d 12:%02dpm", Tm.tm_mon + 1, Tm.tm_mday, Tm.tm_min);
            else if (Tm.tm_hour < 12)
                snprintf(buffer, size, "%d/%d %u:%02dam", Tm.tm_mon + 1, Tm.tm_mday, Tm.tm_hour, Tm.tm_min);
            else if (Tm.tm_hour > 12)
                snprintf(buffer, size, "%d/%d %u:%02dpm", Tm.tm_mon + 1, Tm.tm_mday, Tm.tm_hour - 12, Tm.tm_min);
            break;
        case ImPlotTimeFmt_DayMoYrHrMin:
            if (Tm.tm_hour == 0)
                snprintf(buffer, size, "%d/%d/%d 12:%02dam", Tm.tm_mon + 1, Tm.tm_mday, Tm.tm_year + 1900, Tm.tm_min);
            else if (Tm.tm_hour == 12)
                snprintf(buffer, size, "%d/%d/%d 12:%02dpm", Tm.tm_mon + 1, Tm.tm_mday, Tm.tm_year + 1900, Tm.tm_min);
            else if (Tm.tm_hour < 12)
                snprintf(buffer, size, "%d/%d/%d %u:%02dam", Tm.tm_mon + 1, Tm.tm_mday, Tm.tm_year + 1900, Tm.tm_hour, Tm.tm_min);
            else if (Tm.tm_hour > 12)
                snprintf(buffer, size, "%d/%d/%d %u:%02dpm", Tm.tm_mon + 1, Tm.tm_mday, Tm.tm_year + 1900, Tm.tm_hour - 12, Tm.tm_min);
            break;
        case ImPlotTimeFmt_Hr:
            if (Tm.tm_hour == 0)
                snprintf(buffer, size, "12am");
            else if (Tm.tm_hour == 12)
                snprintf(buffer, size, "12pm");
            else if (Tm.tm_hour < 12)
                snprintf(buffer, size, "%uam", Tm.tm_hour);
            else if (Tm.tm_hour > 12)
                snprintf(buffer, size, "%upm", Tm.tm_hour - 12);
            break;
        case ImPlotTimeFmt_HrMin:
            if (Tm.tm_hour == 0)
                snprintf(buffer, size, "12:%02dam", Tm.tm_min);
            else if (Tm.tm_hour == 12)
                snprintf(buffer, size, "12:%02dpm", Tm.tm_min);
            else if (Tm.tm_hour < 12)
                snprintf(buffer, size, "%d:%02dam", Tm.tm_hour, Tm.tm_min);
            else if (Tm.tm_hour > 12)
                snprintf(buffer, size, "%d:%02dpm", Tm.tm_hour - 12, Tm.tm_min);
            break;
        case ImPlotTimeFmt_S:   snprintf(buffer, size, ":%02d",      Tm.tm_sec);     break;
        case ImPlotTimeFmt_SMs: snprintf(buffer, size, ":%02d.%03d", Tm.tm_sec, ms); break;
        case ImPlotTimeFmt_SUs: snprintf(buffer, size, ":%02d.%06d", Tm.tm_sec, us); break;
        default: break;
    }
}

inline void PrintTime(double t, ImPlotTimeFmt fmt) {
    char buff[32];
    FormatTime(t, buff, 32, fmt);
    printf("%s\n",buff);
}

// Returns the nominally largest possible width for a time format
inline float GetTimeLabelWidth(ImPlotTimeFmt fmt) {
    switch (fmt) {
        case ImPlotTimeFmt_SUs:          return ImGui::CalcTextSize(":88.888888").x;         // :29.428552
        case ImPlotTimeFmt_SMs:          return ImGui::CalcTextSize(":88.888").x;            // :29.428
        case ImPlotTimeFmt_S:            return ImGui::CalcTextSize(":88").x;                // :29
        case ImPlotTimeFmt_HrMin:        return ImGui::CalcTextSize("88:88pm").x;            // 7:21pm
        case ImPlotTimeFmt_Hr:           return ImGui::CalcTextSize("8pm").x;                // 7pm
        case ImPlotTimeFmt_DayMo:        return ImGui::CalcTextSize("88/88").x;              // 10/3
        case ImPlotTimeFmt_DayMoHrMin:   return ImGui::CalcTextSize("88/88 88:88pm").x;      // 10/3 7:21pm
        case ImPlotTimeFmt_DayMoYrHrMin: return ImGui::CalcTextSize("88/88/8888 88:88pm").x; // 10/3/1991 7:21pm
        case ImPlotTimeFmt_Mo:           return ImGui::CalcTextSize("MMM").x;                // Oct
        case ImPlotTimeFmt_Yr:           return ImGui::CalcTextSize("8888").x;               // 1991
        default:                         return 0;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Internal / Experimental Plotters
// No guarantee of forward compatibility here!
//-----------------------------------------------------------------------------

// Plots axis-aligned, filled rectangles. Every two consecutive points defines opposite corners of a single rectangle.
void PlotRects(const char* label_id, const float* xs, const float* ys, int count, int offset = 0, int stride = sizeof(float));
void PlotRects(const char* label_id, const double* xs, const double* ys, int count, int offset = 0, int stride = sizeof(double));
void PlotRects(const char* label_id, ImPlotPoint (*getter)(void* data, int idx), void* data, int count, int offset = 0);

} // namespace ImPlot