#include "face_manager.h"
#include "face_bear.h"
#include "face_rabbit.h"
#include "face_cat.h"
#include "face_heart.h"
#include "settings.h"
#include <esp_log.h>
#include <cstring>

#define TAG "FaceManager"

FaceManager::FaceManager() {
}

FaceManager::~FaceManager() {
    DestroyFace();
    faces_.clear();
}

void FaceManager::Initialize() {
    if (initialized_) {
        return;
    }

    // Create all 4 faces: Bear, Rabbit, Cat, Heart
    faces_.push_back(std::make_unique<BearFace>());
    faces_.push_back(std::make_unique<RabbitFace>());
    faces_.push_back(std::make_unique<CatFace>());
    faces_.push_back(std::make_unique<HeartFace>());

    // Load saved face selection from NVS
    LoadFromNVS();

    initialized_ = true;
    ESP_LOGI(TAG, "FaceManager initialized with %d faces, current: %s",
             (int)faces_.size(), GetCurrentFace()->GetName());
}

void FaceManager::CreateFace(lv_obj_t* parent) {
    if (!initialized_ || faces_.empty()) {
        ESP_LOGE(TAG, "FaceManager not initialized");
        return;
    }

    parent_ = parent;

    // Destroy previous face if any
    DestroyFace();

    // Create current face
    VectorFace* face = GetCurrentFace();
    if (face != nullptr) {
        face->Create(parent);
        face->Update();
        ESP_LOGI(TAG, "Created face: %s", face->GetName());
    }
}

void FaceManager::DestroyFace() {
    for (auto& face : faces_) {
        if (face->IsCreated()) {
            face->Destroy();
        }
    }
}

void FaceManager::NextFace() {
    if (faces_.empty()) return;

    // Destroy current face
    VectorFace* old_face = GetCurrentFace();
    if (old_face != nullptr && old_face->IsCreated()) {
        old_face->Destroy();
    }

    // Move to next face
    current_index_ = (current_index_ + 1) % faces_.size();

    // Create new face
    VectorFace* new_face = GetCurrentFace();
    if (new_face != nullptr && parent_ != nullptr) {
        new_face->Create(parent_);
        new_face->Update();
    }

    // Save to NVS
    SaveToNVS();

    // Notify callback
    if (face_changed_callback_) {
        face_changed_callback_(new_face);
    }

    ESP_LOGI(TAG, "Switched to next face: %s", new_face ? new_face->GetName() : "none");
}

void FaceManager::PreviousFace() {
    if (faces_.empty()) return;

    // Destroy current face
    VectorFace* old_face = GetCurrentFace();
    if (old_face != nullptr && old_face->IsCreated()) {
        old_face->Destroy();
    }

    // Move to previous face (with wrap around)
    current_index_ = (current_index_ + faces_.size() - 1) % faces_.size();

    // Create new face
    VectorFace* new_face = GetCurrentFace();
    if (new_face != nullptr && parent_ != nullptr) {
        new_face->Create(parent_);
        new_face->Update();
    }

    // Save to NVS
    SaveToNVS();

    // Notify callback
    if (face_changed_callback_) {
        face_changed_callback_(new_face);
    }

    ESP_LOGI(TAG, "Switched to previous face: %s", new_face ? new_face->GetName() : "none");
}

bool FaceManager::SwitchToFace(const char* face_id) {
    if (face_id == nullptr || faces_.empty()) {
        return false;
    }

    for (int i = 0; i < (int)faces_.size(); i++) {
        if (strcmp(faces_[i]->GetId(), face_id) == 0) {
            if (i != current_index_) {
                // Destroy current face
                VectorFace* old_face = GetCurrentFace();
                if (old_face != nullptr && old_face->IsCreated()) {
                    old_face->Destroy();
                }

                current_index_ = i;

                // Create new face
                VectorFace* new_face = GetCurrentFace();
                if (new_face != nullptr && parent_ != nullptr) {
                    new_face->Create(parent_);
                    new_face->Update();
                }

                SaveToNVS();

                if (face_changed_callback_) {
                    face_changed_callback_(new_face);
                }

                ESP_LOGI(TAG, "Switched to face: %s", face_id);
            }
            return true;
        }
    }

    ESP_LOGW(TAG, "Face not found: %s", face_id);
    return false;
}

VectorFace* FaceManager::GetCurrentFace() const {
    if (faces_.empty() || current_index_ >= (int)faces_.size()) {
        return nullptr;
    }
    return faces_[current_index_].get();
}

VectorFace* FaceManager::GetFace(int index) const {
    if (index < 0 || index >= (int)faces_.size()) {
        return nullptr;
    }
    return faces_[index].get();
}

bool FaceManager::ProcessSwipe(SwipeDirection direction) {
    switch (direction) {
        case SwipeDirection::kLeft:
            NextFace();
            return true;
        case SwipeDirection::kRight:
            PreviousFace();
            return true;
        default:
            return false;
    }
}

void FaceManager::SetEmotion(const char* emotion) {
    VectorFace* face = GetCurrentFace();
    if (face != nullptr) {
        face->SetEmotion(emotion);
    }
}

void FaceManager::Animate(int frame) {
    VectorFace* face = GetCurrentFace();
    if (face != nullptr && face->IsCreated()) {
        face->Animate(frame);
    }
}

void FaceManager::SaveToNVS() {
    VectorFace* face = GetCurrentFace();
    if (face == nullptr) return;

    Settings settings(kNvsNamespace, true);
    settings.SetString(kNvsKeyCurrentFace, face->GetId());
    ESP_LOGI(TAG, "Saved face to NVS: %s", face->GetId());
}

void FaceManager::LoadFromNVS() {
    Settings settings(kNvsNamespace, false);
    std::string face_id = settings.GetString(kNvsKeyCurrentFace, "bear");

    // Find face by ID
    for (int i = 0; i < (int)faces_.size(); i++) {
        if (strcmp(faces_[i]->GetId(), face_id.c_str()) == 0) {
            current_index_ = i;
            ESP_LOGI(TAG, "Loaded face from NVS: %s", face_id.c_str());
            return;
        }
    }

    // Default to first face if not found
    current_index_ = 0;
    ESP_LOGW(TAG, "Face not found in NVS: %s, defaulting to %s",
             face_id.c_str(), faces_.empty() ? "none" : faces_[0]->GetId());
}
