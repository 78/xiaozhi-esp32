#ifndef FACE_MANAGER_H
#define FACE_MANAGER_H

#include "vector_face.h"
#include "swipe_detector.h"
#include <vector>
#include <memory>
#include <functional>

/**
 * Manages multiple vector faces and handles switching between them.
 * Persists the selected face to NVS for boot-up restoration.
 */
class FaceManager {
public:
    using FaceChangedCallback = std::function<void(VectorFace*)>;

    FaceManager();
    ~FaceManager();

    /**
     * Initialize the face manager with available faces.
     * Must be called before any other methods.
     */
    void Initialize();

    /**
     * Create face UI elements on the given parent.
     * @param parent LVGL parent object for face rendering
     */
    void CreateFace(lv_obj_t* parent);

    /**
     * Destroy current face UI elements.
     */
    void DestroyFace();

    /**
     * Switch to the next face (circular).
     */
    void NextFace();

    /**
     * Switch to the previous face (circular).
     */
    void PreviousFace();

    /**
     * Switch to a specific face by ID.
     * @param face_id Face identifier (e.g., "bear", "cat")
     * @return true if face was found and switched
     */
    bool SwitchToFace(const char* face_id);

    /**
     * Get the currently active face.
     */
    VectorFace* GetCurrentFace() const;

    /**
     * Get the number of available faces.
     */
    int GetFaceCount() const { return faces_.size(); }

    /**
     * Get face by index.
     */
    VectorFace* GetFace(int index) const;

    /**
     * Process swipe gesture for face switching.
     * @param direction Detected swipe direction
     * @return true if face was changed
     */
    bool ProcessSwipe(SwipeDirection direction);

    /**
     * Set the emotion on the current face.
     */
    void SetEmotion(const char* emotion);

    /**
     * Update animation frame on current face.
     */
    void Animate(int frame);

    /**
     * Register callback for face change events.
     */
    void OnFaceChanged(FaceChangedCallback callback) {
        face_changed_callback_ = callback;
    }

    /**
     * Check if faces are initialized.
     */
    bool IsInitialized() const { return initialized_; }

    /**
     * Save current face selection to NVS.
     */
    void SaveToNVS();

    /**
     * Load face selection from NVS.
     */
    void LoadFromNVS();

private:
    std::vector<std::unique_ptr<VectorFace>> faces_;
    int current_index_ = 0;
    bool initialized_ = false;
    lv_obj_t* parent_ = nullptr;

    FaceChangedCallback face_changed_callback_;

    // NVS namespace and key
    static constexpr const char* kNvsNamespace = "face";
    static constexpr const char* kNvsKeyCurrentFace = "current";
};

#endif // FACE_MANAGER_H
