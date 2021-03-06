
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkClipStack_DEFINED
#define SkClipStack_DEFINED

#include "SkDeque.h"
#include "SkPath.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "SkTDArray.h"


// Because a single save/restore state can have multiple clips, this class
// stores the stack depth (fSaveCount) and clips (fDeque) separately.
// Each clip in fDeque stores the stack state to which it belongs
// (i.e., the fSaveCount in force when it was added). Restores are thus
// implemented by removing clips from fDeque that have an fSaveCount larger
// then the freshly decremented count.
class SK_API SkClipStack {
public:
    enum BoundsType {
        // The bounding box contains all the pixels that can be written to
        kNormal_BoundsType,
        // The bounding box contains all the pixels that cannot be written to.
        // The real bound extends out to infinity and all the pixels outside
        // of the bound can be written to. Note that some of the pixels inside
        // the bound may also be writeable but all pixels that cannot be
        // written to are guaranteed to be inside.
        kInsideOut_BoundsType
    };

    class Element {
    public:
        enum Type {
            //!< This element makes the clip empty (regardless of previous elements).
            kEmpty_Type,
            //!< This element combines a rect with the current clip using a set operation
            kRect_Type,
            //!< This element combines a path with the current clip using a set operation
            kPath_Type,
        };

        Element() {
            this->initCommon(0, SkRegion::kReplace_Op, false);
            this->setEmpty();
        }

        Element(const SkRect& rect, SkRegion::Op op, bool doAA) {
            this->initRect(0, rect, op, doAA);
        }

        Element(const SkPath& path, SkRegion::Op op, bool doAA) {
            this->initPath(0, path, op, doAA);
        }

        bool operator== (const Element& element) const {
            if (this == &element) {
                return true;
            }
            if (fOp != element.fOp ||
                fType != element.fType ||
                fDoAA != element.fDoAA ||
                fSaveCount != element.fSaveCount) {
                return false;
            }
            switch (fType) {
                case kPath_Type:
                    return fPath == element.fPath;
                case kRect_Type:
                    return fRect == element.fRect;
                case kEmpty_Type:
                    return true;
                default:
                    SkDEBUGFAIL("Unexpected type.");
                    return false;
            }
        }
        bool operator!= (const Element& element) const { return !(*this == element); }

        //!< Call to get the type of the clip element.
        Type getType() const { return fType; }

        //!< Call if getType() is kPath to get the path.
        const SkPath& getPath() const { return fPath; }

        //!< Call if getType() is kRect to get the rect.
        const SkRect& getRect() const { return fRect; }

        //!< Call if getType() is not kEmpty to get the set operation used to combine this element.
        SkRegion::Op getOp() const { return fOp; }

        /** If getType() is not kEmpty this indicates whether the clip shape should be anti-aliased
            when it is rasterized. */
        bool isAA() const { return fDoAA; }

        //!< Inverts the fill of the clip shape. Note that a kEmpty element remains kEmpty.
        void invertShapeFillType();

        //!< Sets the set operation represented by the element.
        void setOp(SkRegion::Op op) { fOp = op; }

        /** The GenID can be used by clip stack clients to cache representations of the clip. The
            ID corresponds to the set of clip elements up to and including this element within the
            stack not to the element itself. That is the same clip path in different stacks will
            have a different ID since the elements produce different clip result in the context of
            their stacks. */
        int32_t getGenID() const { return fGenID; }

        /**
         * Gets the bounds of the clip element, either the rect or path bounds. (Whether the shape
         * is inverse filled is not considered.)
         */
        const SkRect& getBounds() const {
            static const SkRect kEmpty = { 0, 0, 0, 0 };
            switch (fType) {
                case kRect_Type:
                    return fRect;
                case kPath_Type:
                    return fPath.getBounds();
                case kEmpty_Type:
                    return kEmpty;
                default:
                    SkDEBUGFAIL("Unexpected type.");
                    return kEmpty;
            }
        }

        /**
         * Conservatively checks whether the clip shape contains the rect param. (Whether the shape
         * is inverse filled is not considered.)
         */
        bool contains(const SkRect& rect) const {
            switch (fType) {
                case kRect_Type:
                    return fRect.contains(rect);
                case kPath_Type:
                    return fPath.conservativelyContainsRect(rect);
                case kEmpty_Type:
                    return false;
                default:
                    SkDEBUGFAIL("Unexpected type.");
                    return false;
            }
        }

        /**
         * Is the clip shape inverse filled.
         */
        bool isInverseFilled() const {
            return kPath_Type == fType && fPath.isInverseFillType();
        }

    private:
        friend class SkClipStack;

        SkPath          fPath;
        SkRect          fRect;
        int             fSaveCount; // save count of stack when this element was added.
        SkRegion::Op    fOp;
        Type            fType;
        bool            fDoAA;

        /* fFiniteBoundType and fFiniteBound are used to incrementally update the clip stack's
           bound. When fFiniteBoundType is kNormal_BoundsType, fFiniteBound represents the
           conservative bounding box of the pixels that aren't clipped (i.e., any pixels that can be
           drawn to are inside the bound). When fFiniteBoundType is kInsideOut_BoundsType (which
           occurs when a clip is inverse filled), fFiniteBound represents the conservative bounding
           box of the pixels that _are_ clipped (i.e., any pixels that cannot be drawn to are inside
           the bound). When fFiniteBoundType is kInsideOut_BoundsType the actual bound is the
           infinite plane. This behavior of fFiniteBoundType and fFiniteBound is required so that we
           can capture the cancelling out of the extensions to infinity when two inverse filled
           clips are Booleaned together. */
        SkClipStack::BoundsType fFiniteBoundType;
        SkRect                  fFiniteBound;

        // When element is applied to the previous elements in the stack is the result known to be
        // equivalent to a single rect intersection? IIOW, is the clip effectively a rectangle.
        bool                    fIsIntersectionOfRects;

        int                     fGenID;

        Element(int saveCount) {
            this->initCommon(saveCount, SkRegion::kReplace_Op, false);
            this->setEmpty();
        }

        Element(int saveCount, const SkRect& rect, SkRegion::Op op, bool doAA) {
            this->initRect(saveCount, rect, op, doAA);
        }

        Element(int saveCount, const SkPath& path, SkRegion::Op op, bool doAA) {
            this->initPath(saveCount, path, op, doAA);
        }

        void initCommon(int saveCount, SkRegion::Op op, bool doAA) {
            fSaveCount = saveCount;
            fOp = op;
            fDoAA = doAA;
            // A default of inside-out and empty bounds means the bounds are effectively void as it
            // indicates that nothing is known to be outside the clip.
            fFiniteBoundType = kInsideOut_BoundsType;
            fFiniteBound.setEmpty();
            fIsIntersectionOfRects = false;
            fGenID = kInvalidGenID;
        }

        void initRect(int saveCount, const SkRect& rect, SkRegion::Op op, bool doAA) {
            fRect = rect;
            fType = kRect_Type;
            this->initCommon(saveCount, op, doAA);
        }

        void initPath(int saveCount, const SkPath& path, SkRegion::Op op, bool doAA) {
            fPath = path;
            fType = kPath_Type;
            this->initCommon(saveCount, op, doAA);
        }

        void setEmpty() {
            fType = kEmpty_Type;
            fFiniteBound.setEmpty();
            fFiniteBoundType = kNormal_BoundsType;
            fIsIntersectionOfRects = false;
            fRect.setEmpty();
            fPath.reset();
            fGenID = kEmptyGenID;
        }

        // All Element methods below are only used within SkClipStack.cpp
        inline void checkEmpty() const;
        inline bool canBeIntersectedInPlace(int saveCount, SkRegion::Op op) const;
        /* This method checks to see if two rect clips can be safely merged into one. The issue here
          is that to be strictly correct all the edges of the resulting rect must have the same
          anti-aliasing. */
        bool rectRectIntersectAllowed(const SkRect& newR, bool newAA) const;
        /** Determines possible finite bounds for the Element given the previous element of the
            stack */
        void updateBoundAndGenID(const Element* prior);
        // The different combination of fill & inverse fill when combining bounding boxes
        enum FillCombo {
            kPrev_Cur_FillCombo,
            kPrev_InvCur_FillCombo,
            kInvPrev_Cur_FillCombo,
            kInvPrev_InvCur_FillCombo
        };
        // per-set operation functions used by updateBoundAndGenID().
        inline void combineBoundsDiff(FillCombo combination, const SkRect& prevFinite);
        inline void combineBoundsXOR(int combination, const SkRect& prevFinite);
        inline void combineBoundsUnion(int combination, const SkRect& prevFinite);
        inline void combineBoundsIntersection(int combination, const SkRect& prevFinite);
        inline void combineBoundsRevDiff(int combination, const SkRect& prevFinite);
    };

    SkClipStack();
    SkClipStack(const SkClipStack& b);
    explicit SkClipStack(const SkRect& r);
    explicit SkClipStack(const SkIRect& r);
    ~SkClipStack();

    SkClipStack& operator=(const SkClipStack& b);
    bool operator==(const SkClipStack& b) const;
    bool operator!=(const SkClipStack& b) const { return !(*this == b); }

    void reset();

    int getSaveCount() const { return fSaveCount; }
    void save();
    void restore();

    /**
     * getBounds places the current finite bound in its first parameter. In its
     * second, it indicates which kind of bound is being returned. If
     * 'canvFiniteBound' is a normal bounding box then it encloses all writeable
     * pixels. If 'canvFiniteBound' is an inside out bounding box then it
     * encloses all the un-writeable pixels and the true/normal bound is the
     * infinite plane. isIntersectionOfRects is an optional parameter
     * that is true if 'canvFiniteBound' resulted from an intersection of rects.
     */
    void getBounds(SkRect* canvFiniteBound,
                   BoundsType* boundType,
                   bool* isIntersectionOfRects = NULL) const;

    /**
     * Takes an input rect in device space and conservatively clips it to the
     * clip-stack. If false is returned then the rect does not intersect the
     * clip and is unmodified.
     */
    bool intersectRectWithClip(SkRect* devRect) const;

    /**
     * Returns true if the input rect in device space is entirely contained
     * by the clip. A return value of false does not guarantee that the rect
     * is not contained by the clip.
     */
    bool quickContains(const SkRect& devRect) const;

    void clipDevRect(const SkIRect& ir, SkRegion::Op op) {
        SkRect r;
        r.set(ir);
        this->clipDevRect(r, op, false);
    }
    void clipDevRect(const SkRect&, SkRegion::Op, bool doAA);
    void clipDevPath(const SkPath&, SkRegion::Op, bool doAA);
    // An optimized version of clipDevRect(emptyRect, kIntersect, ...)
    void clipEmpty();

    /**
     * isWideOpen returns true if the clip state corresponds to the infinite
     * plane (i.e., draws are not limited at all)
     */
    bool isWideOpen() const;

    /**
     * Add a callback function that will be called whenever a clip state
     * is no longer viable. This will occur whenever restore
     * is called or when a clipDevRect or clipDevPath call updates the
     * clip within an existing save/restore state. Each clip state is
     * represented by a unique generation ID.
     */
    typedef void (*PFPurgeClipCB)(int genID, void* data);
    void addPurgeClipCallback(PFPurgeClipCB callback, void* data) const;

    /**
     * Remove a callback added earlier via addPurgeClipCallback
     */
    void removePurgeClipCallback(PFPurgeClipCB callback, void* data) const;

    /**
     * The generation ID has three reserved values to indicate special
     * (potentially ignorable) cases
     */
    static const int32_t kInvalidGenID = 0;
    static const int32_t kEmptyGenID = 1;       // no pixels writeable
    static const int32_t kWideOpenGenID = 2;    // all pixels writeable

    int32_t getTopmostGenID() const;

public:
    class Iter {
    public:
        enum IterStart {
            kBottom_IterStart = SkDeque::Iter::kFront_IterStart,
            kTop_IterStart = SkDeque::Iter::kBack_IterStart
        };

        /**
         * Creates an uninitialized iterator. Must be reset()
         */
        Iter();

        Iter(const SkClipStack& stack, IterStart startLoc);

        /**
         *  Return the clip element for this iterator. If next()/prev() returns NULL, then the
         *  iterator is done.
         */
        const Element* next();
        const Element* prev();

        /**
         * Moves the iterator to the topmost element with the specified RegionOp and returns that
         * element. If no clip element with that op is found, the first element is returned.
         */
        const Element* skipToTopmost(SkRegion::Op op);

        /**
         * Restarts the iterator on a clip stack.
         */
        void reset(const SkClipStack& stack, IterStart startLoc);

    private:
        const SkClipStack* fStack;
        SkDeque::Iter      fIter;
    };

    /**
     * The B2TIter iterates from the bottom of the stack to the top.
     * It inherits privately from Iter to prevent access to reverse iteration.
     */
    class B2TIter : private Iter {
    public:
        B2TIter() {}

        /**
         * Wrap Iter's 2 parameter ctor to force initialization to the
         * beginning of the deque/bottom of the stack
         */
        B2TIter(const SkClipStack& stack)
        : INHERITED(stack, kBottom_IterStart) {
        }

        using Iter::next;

        /**
         * Wrap Iter::reset to force initialization to the
         * beginning of the deque/bottom of the stack
         */
        void reset(const SkClipStack& stack) {
            this->INHERITED::reset(stack, kBottom_IterStart);
        }

    private:

        typedef Iter INHERITED;
    };

    /**
     * GetConservativeBounds returns a conservative bound of the current clip.
     * Since this could be the infinite plane (if inverse fills were involved) the
     * maxWidth and maxHeight parameters can be used to limit the returned bound
     * to the expected drawing area. Similarly, the offsetX and offsetY parameters
     * allow the caller to offset the returned bound to account for translated
     * drawing areas (i.e., those resulting from a saveLayer). For finite bounds,
     * the translation (+offsetX, +offsetY) is applied before the clamp to the
     * maximum rectangle: [0,maxWidth) x [0,maxHeight).
     * isIntersectionOfRects is an optional parameter that is true when
     * 'devBounds' is the result of an intersection of rects. In this case
     * 'devBounds' is the exact answer/clip.
     */
    void getConservativeBounds(int offsetX,
                               int offsetY,
                               int maxWidth,
                               int maxHeight,
                               SkRect* devBounds,
                               bool* isIntersectionOfRects = NULL) const;

private:
    friend class Iter;

    SkDeque fDeque;
    int     fSaveCount;

    // Generation ID for the clip stack. This is incremented for each
    // clipDevRect and clipDevPath call. 0 is reserved to indicate an
    // invalid ID.
    static int32_t     gGenID;

    struct ClipCallbackData {
        PFPurgeClipCB   fCallback;
        void*           fData;

        friend bool operator==(const ClipCallbackData& a,
                               const ClipCallbackData& b) {
            return a.fCallback == b.fCallback && a.fData == b.fData;
        }
    };

    mutable SkTDArray<ClipCallbackData> fCallbackData;

    /**
     * Invoke all the purge callbacks passing in element's generation ID.
     */
    void purgeClip(Element* element);

    /**
     * Return the next unique generation ID.
     */
    static int32_t GetNextGenID();
};

#endif

