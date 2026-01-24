#ifdef LINUX_BUILD

#include "Editor/EditorUI.h"
#include "Editor/EditorSystem.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Components/MeshRenderer.h"
#include "Components/ModelRenderer.h"
#include "Components/LightComponent.h"
#include "Components/CameraComponent.h"
#include "Components/PhysicsComponent.h"
#include "Components/Area3DComponent.h"
#include "Components/TextComponent.h"
#include "Components/ScriptComponent.h"
#include "Components/AnimationComponent.h"
#include "Core/Engine.h"
#include "Editor/BuildSystem.h"
#include "Editor/SceneSerializer.h"
#include "Editor/FileDialog.h"
#include "Rendering/Framebuffer.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Rendering/Texture.h"
#include "Physics/PhysicsManager.h"
#include "Input/InputMapping.h"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <memory>

// ImGui includes
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

// ImGuizmo includes
#include "../../vendor/imguizmo/ImGuizmo.h"

namespace GameEngine {

EditorUI::EditorUI(EditorSystem& editorSystem)
    : editor(editorSystem)
    , showDemoWindow(false)
    , showSceneGraph(true)
    , showProperties(true)
    , showViewport(true)
    , showFileExplorer(false)
    , sceneGraphWidth(300.0f)
    , propertiesWidth(350.0f)
    , fileExplorerHeight(200.0f)
    , expandAllNodes(false)
    , collapseAllNodes(false)
    , focusOnSelectedNode(false)
{
}

void EditorUI::setupDockspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    
    ImGui::End();
}

void EditorUI::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                editor.createNewScene();
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                std::string filepath = FileDialog::openFileDialog("Open Scene", "*.json");
                if (FileDialog::isValidResult(filepath)) {
                    if (editor.loadSceneFromFile(filepath)) {
                        std::cout << "Scene loaded from: " << filepath << std::endl;
                    } else {
                        std::cout << "Failed to load scene from: " << filepath << std::endl;
                    }
                }
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                std::string filepath = FileDialog::getDefaultScenesDirectory() + "/scene.json";
                if (editor.saveSceneToFile(filepath)) {
                    std::cout << "Scene saved to: " << filepath << std::endl;
                }
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                std::string filepath = FileDialog::saveFileDialog("Save Scene As", "*.json", "scene.json");
                if (FileDialog::isValidResult(filepath)) {
                    if (editor.saveSceneToFile(filepath)) {
                        std::cout << "Scene saved to: " << filepath << std::endl;
                    } else {
                        std::cout << "Failed to save scene to: " << filepath << std::endl;
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                GetEngine().setRunning(false);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Scene Graph", nullptr, &showSceneGraph);
            ImGui::MenuItem("Properties", nullptr, &showProperties);
            ImGui::MenuItem("Viewport", nullptr, &showViewport);
            ImGui::MenuItem("File Explorer", nullptr, &showFileExplorer);
            ImGui::MenuItem("Input Mapping", nullptr, &showInputMapping);
            ImGui::Separator();
            ImGui::MenuItem("Demo Window", nullptr, &showDemoWindow);
            
            bool physicsDebugEnabled = PhysicsManager::getInstance().isDebugDrawEnabled();
            if (ImGui::MenuItem("Physics Debug Shapes", nullptr, &physicsDebugEnabled)) {
                PhysicsManager::getInstance().setDebugDrawEnabled(physicsDebugEnabled);
            }
            
            ImGui::Separator();
            if (ImGui::MenuItem("Disable All Physics Debug")) {
                PhysicsManager::getInstance().setDebugDrawEnabled(false);
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Empty Node")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto node = scene->createNode(editor.generateUniqueNodeName("Node"));
                    auto selected = editor.getSelectedNode();
                    if (selected) {
                        selected->addChild(node);
                    } else {
                        scene->getRootNode()->addChild(node);
                    }
                }
            }
            
            if (ImGui::MenuItem("Camera")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto node = scene->createNode(editor.generateUniqueNodeName("Camera"));
                    node->addComponent<CameraComponent>();
                    scene->getRootNode()->addChild(node);
                    scene->setActiveCamera(node);
                }
            }
            
            if (ImGui::MenuItem("Text")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto node = scene->createNode(editor.generateUniqueNodeName("Text"));
                    auto textComponent = node->addComponent<TextComponent>();
                    textComponent->setText("Hello World!");
                    textComponent->setFontPath("assets/fonts/DroidSans.ttf");
                    textComponent->setFontSize(32.0f);
                    textComponent->setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                    textComponent->setRenderMode(TextRenderMode::WORLD_SPACE);
                    textComponent->setAlignment(TextAlignment::CENTER);
                    textComponent->start();
                    
                    auto selected = editor.getSelectedNode();
                    if (selected) {
                        selected->addChild(node);
                    } else {
                        scene->getRootNode()->addChild(node);
                    }
                }
            }
            
            if (ImGui::MenuItem("Area3D")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto node = scene->createNode(editor.generateUniqueNodeName("Area3D"));
                    auto area3DComponent = node->addComponent<Area3DComponent>();
                    area3DComponent->start();
                    
                    auto selected = editor.getSelectedNode();
                    if (selected) {
                        selected->addChild(node);
                    } else {
                        scene->getRootNode()->addChild(node);
                    }
                }
            }
            
            if (ImGui::BeginMenu("Mesh Shapes")) {
                if (ImGui::MenuItem("Plane")) { 
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("PlaneMesh"));
                        auto meshRenderer = node->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createPlane(1.0f, 1.0f, 1));

                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                        meshRenderer->setMaterial(material);
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                    }
                }

                if (ImGui::MenuItem("Cube")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("CubeMesh"));
                        auto meshRenderer = node->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createCube());
                        
                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                        meshRenderer->setMaterial(material);
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                    }
                }
                
                if (ImGui::MenuItem("Sphere")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("SphereMesh"));
                        auto meshRenderer = node->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createSphere(32, 16));

                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                        meshRenderer->setMaterial(material);
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                    }
                }
                
                if (ImGui::MenuItem("Capsule")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("CapsuleMesh"));
                        auto meshRenderer = node->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createCapsule(0.5f, 0.5f));

                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                        meshRenderer->setMaterial(material);
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                    }
                }
                
                if (ImGui::MenuItem("Cylinder")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("CylinderMesh"));
                        auto meshRenderer = node->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createCylinder(0.5f, 1.0f));

                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                        meshRenderer->setMaterial(material);
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                    }
                }
                
                ImGui::EndMenu();
            }
            
            if (ImGui::MenuItem("3D Model")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto node = scene->createNode(editor.generateUniqueNodeName("Model"));
                    node->addComponent<ModelRenderer>();
                    
                    auto selected = editor.getSelectedNode();
                    if (selected) {
                        selected->addChild(node);
                    } else {
                        scene->getRootNode()->addChild(node);
                    }
                    
                    editor.selectNode(node);
                }
            }
            
            if (ImGui::BeginMenu("Collision Shapes")) {
                if (ImGui::MenuItem("Box Collision")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("BoxCollision"));
                        auto physicsComponent = node->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::BOX);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                    }
                }
                
                if (ImGui::MenuItem("Sphere Collision")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("SphereCollision"));
                        auto physicsComponent = node->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::SPHERE);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                    }
                }
                
                if (ImGui::MenuItem("Capsule Collision")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("CapsuleCollision"));
                        auto physicsComponent = node->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::CAPSULE);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                    }
                }
                
                if (ImGui::MenuItem("Cylinder Collision")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("CylinderCollision"));
                        auto physicsComponent = node->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::CYLINDER);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                    }
                }
                
                if (ImGui::MenuItem("Plane Collision")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("PlaneCollision"));
                        auto physicsComponent = node->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::PLANE);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                    }
                }
                
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Complete Entity")) {
                if (ImGui::MenuItem("Box Entity")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto parentNode = scene->createNode(editor.generateUniqueNodeName("BoxEntity"));
                        
                        auto meshNode = scene->createNode("Mesh");
                        auto meshRenderer = meshNode->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createCube());
                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                        meshRenderer->setMaterial(material);
                        parentNode->addChild(meshNode);
                        
                        auto collisionNode = scene->createNode("Collision");
                        auto physicsComponent = collisionNode->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::BOX);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        parentNode->addChild(collisionNode);
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(parentNode);
                        } else {
                            scene->getRootNode()->addChild(parentNode);
                        }
                    }
                }
                
                if (ImGui::MenuItem("Sphere Entity")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto parentNode = scene->createNode(editor.generateUniqueNodeName("SphereEntity"));
                        
                        auto meshNode = scene->createNode("Mesh");
                        auto meshRenderer = meshNode->addComponent<MeshRenderer>();
                        meshRenderer->setMesh(Mesh::createSphere(32, 16));
                        auto material = std::make_shared<Material>();
                        material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                        meshRenderer->setMaterial(material);
                        parentNode->addChild(meshNode);
                        
                        auto collisionNode = scene->createNode("Collision");
                        auto physicsComponent = collisionNode->addComponent<PhysicsComponent>();
                        physicsComponent->setCollisionShape(CollisionShapeType::SPHERE);
                        physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC);
                        physicsComponent->setShowCollisionShape(true);
                        physicsComponent->start();
                        parentNode->addChild(collisionNode);
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(parentNode);
                        } else {
                            scene->getRootNode()->addChild(parentNode);
                        }
                    }
                }
                
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Lights")) {
                if (ImGui::MenuItem("Point Light")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("PointLight"));
                        auto lightComponent = node->addComponent<LightComponent>();
                        lightComponent->setType(LightType::POINT);
                        lightComponent->setColor(glm::vec3(1.0f, 0.8f, 0.6f));
                        lightComponent->setIntensity(2.0f);
                        lightComponent->setRange(8.0f);
                        lightComponent->setShowGizmo(true);
                        node->getTransform().setPosition(glm::vec3(0.0f, 5.0f, 0.0f));
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                        
                        lightComponent->start();
                    }
                }
                
                if (ImGui::MenuItem("Directional Light")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("DirectionalLight"));
                        auto lightComponent = node->addComponent<LightComponent>();
                        lightComponent->setType(LightType::DIRECTIONAL);
                        lightComponent->setColor(glm::vec3(0.8f, 0.9f, 0.6f)); // Cool white
                        lightComponent->setIntensity(1.5f);
                        lightComponent->setDirection(glm::vec3(-0.5f, -1.0f, -0.3f));
                        lightComponent->setShowGizmo(true);
                        node->getTransform().setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                        
                        lightComponent->start();
                    }
                }
                
                if (ImGui::MenuItem("Spot Light")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto node = scene->createNode(editor.generateUniqueNodeName("SpotLight"));
                        auto lightComponent = node->addComponent<LightComponent>();
                        lightComponent->setType(LightType::SPOT);
                        lightComponent->setColor(glm::vec3(1.0f, 0.6f, 0.8f)); // Pink
                        lightComponent->setIntensity(3.0f);
                        lightComponent->setRange(12.0f);
                        lightComponent->setDirection(glm::vec3(0.0f, -1.0f, 0.0f));
                        lightComponent->setCutOff(glm::radians(25.0f));
                        lightComponent->setOuterCutOff(glm::radians(35.0f));
                        lightComponent->setShowGizmo(true);
                        node->getTransform().setPosition(glm::vec3(0.0f, 8.0f, 0.0f));
                        
                        auto selected = editor.getSelectedNode();
                        if (selected) {
                            selected->addChild(node);
                        } else {
                            scene->getRootNode()->addChild(node);
                        }
                        
                        lightComponent->start();
                    }
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginPopupModal("No Node Selected", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Please select a node first to create a child.");
            if (ImGui::Button("OK")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginMenu("Build")) {
            if (ImGui::MenuItem("Build for Linux")) {
                BuildSystem::buildForLinux();
            }
            if (ImGui::MenuItem("Build VPK for PS Vita")) {
                BuildSystem::buildForVita();
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Scene")) {
            if (ImGui::MenuItem("Save Scene to Game")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    SceneSerializer::saveSceneToGame(scene);
                }
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void EditorUI::renderSceneGraph() {
    ImGui::Begin("Scene Graph", &showSceneGraph);
    
    auto scene = editor.getActiveScene();
    if (scene) {
        ImGui::Text("Scene: %s", scene->getName().c_str());
        ImGui::Separator();
        
        if (ImGui::Button("Expand All")) {
            expandAllNodes = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Collapse All")) {
            collapseAllNodes = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Focus on Selected")) {
            focusOnSelectedNode = true;
        }
        
        ImGui::Separator();
        
        auto rootNode = scene->getRootNode();
        if (rootNode) {
            renderSceneNode(rootNode, 0);
        }
    } else {
        ImGui::Text("No active scene");
    }
    
    ImGui::End();
}

void EditorUI::renderSceneNode(std::shared_ptr<SceneNode> node, int depth) {
    if (!node) return;
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    
    if (node == editor.getSelectedNode()) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    if (node->getChildCount() == 0) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    
    std::string nodeLabel = node->getName();
    if (node->getComponent<ModelRenderer>()) {
        nodeLabel = "[MODEL] " + nodeLabel;
    } else if (node->getComponent<MeshRenderer>()) {
        nodeLabel = "[MESH] " + nodeLabel;
    } else if (node->getComponent<CameraComponent>()) {
        nodeLabel = "[CAM] " + nodeLabel;
    } else if (node->getComponent<LightComponent>()) {
        nodeLabel = "[LIGHT] " + nodeLabel;
    } else if (node->getComponent<PhysicsComponent>()) {
        nodeLabel = "[PHYS] " + nodeLabel; // Collision/physics node
    } else if (node->getChildCount() > 0) {
        nodeLabel = "[NODE] " + nodeLabel;
    } else {
        nodeLabel = "[OBJ] " + nodeLabel;
    }
    
    bool nodeOpen = ImGui::TreeNodeEx(nodeLabel.c_str(), flags);
    
    if (ImGui::IsItemClicked()) {
        editor.selectNode(node);
    }
    
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::BeginMenu("Create")) {
                if (ImGui::MenuItem("Empty Node")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Node"));
                        node->addChild(child);
                    }
                }
                
                if (ImGui::MenuItem("Camera")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Camera"));
                        child->addComponent<CameraComponent>();
                        node->addChild(child);
                    }
                }
                
                if (ImGui::MenuItem("Text")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Text"));
                        auto textComponent = child->addComponent<TextComponent>();
                        textComponent->setText("Hello World!");
                        textComponent->setFontPath("assets/fonts/DroidSans.ttf");
                        textComponent->setFontSize(32.0f);
                        textComponent->setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                        textComponent->setRenderMode(TextRenderMode::WORLD_SPACE);
                        textComponent->setAlignment(TextAlignment::CENTER);
                        textComponent->start();
                        node->addChild(child);
                    }
                }
                
                if (ImGui::MenuItem("Area3D")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Area3D"));
                        auto area3DComponent = child->addComponent<Area3DComponent>();
                        area3DComponent->start();
                        node->addChild(child);
                    }
                }
                
                if (ImGui::BeginMenu("Mesh Shapes")) {
                    if (ImGui::MenuItem("Plane")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("PlaneMesh"));
                            auto meshRenderer = child->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createPlane(1.0f, 1.0f, 1));
                            
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                            meshRenderer->setMaterial(material);
                            
                            node->addChild(child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Cube")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("CubeMesh"));
                            auto meshRenderer = child->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createCube());
                            
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                            meshRenderer->setMaterial(material);
                            
                            node->addChild(child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Sphere")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("SphereMesh"));
                            auto meshRenderer = child->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createSphere(32, 16));
                            
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                            meshRenderer->setMaterial(material);
                            
                            node->addChild(child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Capsule")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("CapsuleMesh"));
                            auto meshRenderer = child->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createCapsule(0.5f, 0.5f));
                            
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                            meshRenderer->setMaterial(material);
                            
                            node->addChild(child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Cylinder")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("CylinderMesh"));
                            auto meshRenderer = child->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createCylinder(0.5f, 1.0f));
                            
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                            meshRenderer->setMaterial(material);
                            
                            node->addChild(child);
                        }
                    }
                    
                    ImGui::EndMenu();
                }
                
                if (ImGui::BeginMenu("Complete Entity")) {
                    if (ImGui::MenuItem("Box Entity")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto parentNode = scene->createNode(editor.generateUniqueNodeName("BoxEntity"));
                            
                            auto meshNode = scene->createNode("Mesh");
                            auto meshRenderer = meshNode->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createCube());
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                        meshRenderer->setMaterial(material);
                        parentNode->addChild(meshNode);
                        
                        auto collisionNode = scene->createNode("Collision");
                            auto physicsComponent = collisionNode->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::BOX);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            parentNode->addChild(collisionNode);
                            
                            // Add parent to selected node or root
                            auto selected = editor.getSelectedNode();
                            if (selected) {
                                selected->addChild(parentNode);
                            } else {
                                scene->getRootNode()->addChild(parentNode);
                            }
                        }
                    }
                    
                    if (ImGui::MenuItem("Sphere Entity")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                        auto parentNode = scene->createNode(editor.generateUniqueNodeName("SphereEntity"));
                        
                        auto meshNode = scene->createNode("Mesh");
                            auto meshRenderer = meshNode->addComponent<MeshRenderer>();
                            meshRenderer->setMesh(Mesh::createSphere(32, 16));
                            auto material = std::make_shared<Material>();
                            material->setColor(glm::vec3(1.0f, 0.5f, 0.2f));
                        meshRenderer->setMaterial(material);
                        parentNode->addChild(meshNode);
                        
                        auto collisionNode = scene->createNode("Collision");
                            auto physicsComponent = collisionNode->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::SPHERE);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            parentNode->addChild(collisionNode);
                            
                            // Add parent to selected node or root
                            auto selected = editor.getSelectedNode();
                            if (selected) {
                                selected->addChild(parentNode);
                            } else {
                                scene->getRootNode()->addChild(parentNode);
                            }
                        }
                    }
                    
                    ImGui::EndMenu();
                }
                
                // Lights submenu
                if (ImGui::BeginMenu("Lights")) {
                    if (ImGui::MenuItem("Point Light")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("PointLight"));
                            auto lightComponent = child->addComponent<LightComponent>();
                            lightComponent->setType(LightType::POINT);
                            lightComponent->setColor(glm::vec3(1.0f, 0.8f, 0.6f));
                            lightComponent->setIntensity(2.0f);
                            lightComponent->setRange(8.0f);
                            lightComponent->setShowGizmo(true);
                            child->getTransform().setPosition(glm::vec3(0.0f, 5.0f, 0.0f));
                            node->addChild(child);
                            lightComponent->start();
                        }
                    }
                    
                    if (ImGui::MenuItem("Directional Light")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("DirectionalLight"));
                            auto lightComponent = child->addComponent<LightComponent>();
                            lightComponent->setType(LightType::DIRECTIONAL);
                            lightComponent->setColor(glm::vec3(0.8f, 0.9f, 0.6f));
                            lightComponent->setIntensity(1.5f);
                            lightComponent->setDirection(glm::vec3(-0.5f, -1.0f, -0.3f));
                            lightComponent->setShowGizmo(true);
                            child->getTransform().setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
                            node->addChild(child);
                            lightComponent->start();
                        }
                    }
                    
                    if (ImGui::MenuItem("Spot Light")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("SpotLight"));
                            auto lightComponent = child->addComponent<LightComponent>();
                            lightComponent->setType(LightType::SPOT);
                            lightComponent->setColor(glm::vec3(1.0f, 0.6f, 0.8f));
                            lightComponent->setIntensity(3.0f);
                            lightComponent->setRange(12.0f);
                            lightComponent->setDirection(glm::vec3(0.0f, -1.0f, 0.0f));
                            lightComponent->setCutOff(glm::radians(25.0f));
                            lightComponent->setOuterCutOff(glm::radians(35.0f));
                            lightComponent->setShowGizmo(true);
                            child->getTransform().setPosition(glm::vec3(0.0f, 8.0f, 0.0f));
                            node->addChild(child);
                            lightComponent->start();
                        }
                    }
                    
                    ImGui::EndMenu();
                }
                
                if (ImGui::MenuItem("3D Model")) {
                    auto scene = editor.getActiveScene();
                    if (scene) {
                        auto child = scene->createNode(editor.generateUniqueNodeName("Model"));
                        child->addComponent<ModelRenderer>();
                        node->addChild(child);
                        editor.selectNode(child);
                    }
                }
                
                if (ImGui::BeginMenu("Collision Shapes")) {
                    if (ImGui::MenuItem("Box Collision")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("BoxCollision"));
                            auto physicsComponent = child->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::BOX);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            node->addChild(child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Sphere Collision")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("SphereCollision"));
                            auto physicsComponent = child->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::SPHERE);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            node->addChild(child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Capsule Collision")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("CapsuleCollision"));
                            auto physicsComponent = child->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::CAPSULE);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            node->addChild(child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Cylinder Collision")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("CylinderCollision"));
                            auto physicsComponent = child->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::CYLINDER);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            node->addChild(child);
                        }
                    }
                    
                    if (ImGui::MenuItem("Plane Collision")) {
                        auto scene = editor.getActiveScene();
                        if (scene) {
                            auto child = scene->createNode(editor.generateUniqueNodeName("PlaneCollision"));
                            auto physicsComponent = child->addComponent<PhysicsComponent>();
                            physicsComponent->setCollisionShape(CollisionShapeType::PLANE);
                            physicsComponent->setBodyType(PhysicsBodyType::KINEMATIC); // Changed from STATIC to KINEMATIC
                            physicsComponent->setShowCollisionShape(true);
                            physicsComponent->start();
                            node->addChild(child);
                        }
                    }
                    
                    ImGui::EndMenu();
                }
                
                ImGui::EndMenu();
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Duplicate")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto duplicate = scene->createNode(editor.generateUniqueNodeName(node->getName() + "_Copy"));
                    duplicate->getTransform() = node->getTransform();
                    
                    const auto& components = node->getAllComponents();
                    for (const auto& component : components) {
                        if (component && component->isEnabled()) {
                            if (component->getTypeName() == "MeshRenderer") {
                                auto meshRenderer = duplicate->addComponent<MeshRenderer>();
                                auto originalMeshRenderer = node->getComponent<MeshRenderer>();
                                if (originalMeshRenderer) {
                                    meshRenderer->setMesh(originalMeshRenderer->getMesh());
                                    meshRenderer->setMaterial(originalMeshRenderer->getMaterial());
                                }
                            }
                        }
                    }
                    
                    if (node->getParent()) {
                        node->getParent()->addChild(duplicate);
                    } else {
                        scene->getRootNode()->addChild(duplicate);
                    }
                }
            }
            
            if (ImGui::MenuItem("Delete")) {
                editor.deleteNode(node);
            }
            ImGui::EndPopup();
        }
    
    if (nodeOpen) {
        for (size_t i = 0; i < node->getChildCount(); ++i) {
            renderSceneNode(node->getChild(i), depth + 1);
        }
        ImGui::TreePop();
    }
}

void EditorUI::renderProperties() {
    ImGui::Begin("Properties", &showProperties);
    
    auto selected = editor.getSelectedNode();
    if (selected) {
        static char nodeNameBuffer[256];
        strncpy(nodeNameBuffer, selected->getName().c_str(), sizeof(nodeNameBuffer) - 1);
        nodeNameBuffer[sizeof(nodeNameBuffer) - 1] = '\0';
        
        if (ImGui::InputText("Node Name", nodeNameBuffer, sizeof(nodeNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            selected->setName(nodeNameBuffer);
        }
        
        ImGui::Separator();
        
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& transform = selected->getTransform();
            
            auto physicsComp = selected->getComponent<PhysicsComponent>();
            if (physicsComp) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Yellow warning color
                ImGui::TextWrapped("WARNING: This node has a PhysicsComponent!");
                ImGui::TextWrapped("Moving this node directly can cause bugs. Consider moving the parent node instead.");
                ImGui::PopStyleColor();
                ImGui::Separator();
            }
            
            glm::vec3 position = transform.getPosition();
            if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
                transform.setPosition(position);
            }
            
            glm::vec3 rotation = transform.getEulerAngles();
            if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f)) {
                transform.setEulerAngles(rotation);
            }
            
            glm::vec3 scale = transform.getScale();
            if (ImGui::DragFloat3("Scale", &scale.x, 0.1f)) {
                transform.setScale(scale);
            }
        }
        
        if (ImGui::CollapsingHeader("Node Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool visible = selected->isVisible();
            if (ImGui::Checkbox("Visible", &visible)) {
                selected->setVisible(visible);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Controls whether the node is rendered (drawn on screen).");
                ImGui::Separator();
                ImGui::Text("Checked: Node is visible and rendered");
                ImGui::Text("Unchecked: Node is hidden (not rendered)");
                ImGui::Separator();
                ImGui::Text("Note: Node can still update when hidden.");
                ImGui::Text("Use this to hide/show UI elements or objects.");
                ImGui::EndTooltip();
            }
            
            bool active = selected->isActive();
            if (ImGui::Checkbox("Active", &active)) {
                selected->setActive(active);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Controls whether the node updates AND renders.");
                ImGui::Separator();
                ImGui::Text("Checked: Node updates and renders (if also visible)");
                ImGui::Text("Unchecked: Node is frozen (no updates, no rendering)");
                ImGui::Separator();
                ImGui::Text("Combinations:");
                ImGui::BulletText("Visible=ON, Active=ON: Fully functional");
                ImGui::BulletText("Visible=OFF, Active=ON: Hidden but updating");
                ImGui::BulletText("Visible=ON, Active=OFF: Visible but frozen");
                ImGui::BulletText("Visible=OFF, Active=OFF: Completely disabled");
                ImGui::EndTooltip();
            }
        }
        
        if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Parent: %s", selected->getParent() ? selected->getParent()->getName().c_str() : "None (Root)");
            ImGui::Text("Children: %zu", selected->getChildCount());
            
            if (ImGui::Button("Add Empty Child")) {
                auto scene = editor.getActiveScene();
                if (scene) {
                    auto child = scene->createNode(editor.generateUniqueNodeName("Child"));
                    selected->addChild(child);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove from Parent")) {
                if (selected->getParent()) {
                    selected->getParent()->removeChild(selected->getName());
                }
            }
        }
        
        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Add Component")) {
                ImGui::OpenPopup("AddComponentPopup");
            }
            
            if (ImGui::BeginPopup("AddComponentPopup")) {
                if (ImGui::MenuItem("Camera Component")) {
                    if (!selected->getComponent<CameraComponent>()) {
                        selected->addComponent<CameraComponent>();
                    }
                }
                if (ImGui::MenuItem("Mesh Renderer")) {
                    if (!selected->getComponent<MeshRenderer>()) {
                        selected->addComponent<MeshRenderer>();
                    }
                }
                if (ImGui::MenuItem("Model Renderer")) {
                    if (!selected->getComponent<ModelRenderer>()) {
                        selected->addComponent<ModelRenderer>();
                    }
                }
                if (ImGui::MenuItem("Animation Component")) {
                    if (!selected->getComponent<AnimationComponent>()) {
                        auto animComponent = selected->addComponent<AnimationComponent>();
                        animComponent->start(); // Initialize the animation component
                    }
                }
                if (ImGui::MenuItem("Light Component")) {
                    if (!selected->getComponent<LightComponent>()) {
                        selected->addComponent<LightComponent>();
                    }
                }
                if (ImGui::MenuItem("Physics Component")) {
                    if (!selected->getComponent<PhysicsComponent>()) {
                        selected->addComponent<PhysicsComponent>();
                        // Warning is shown in Transform section when PhysicsComponent is detected
                    }
                }
                if (ImGui::MenuItem("Area3D Component")) {
                    if (!selected->getComponent<Area3DComponent>()) {
                        auto area3DComponent = selected->addComponent<Area3DComponent>();
                        area3DComponent->start(); // Initialize the area3D component
                    }
                }
                if (ImGui::MenuItem("Text Component")) {
                    if (!selected->getComponent<TextComponent>()) {
                        auto textComponent = selected->addComponent<TextComponent>();
                        textComponent->setText("Hello World!");
                        textComponent->setFontPath("assets/fonts/DroidSans.ttf");
                        textComponent->setFontSize(32.0f);
                        textComponent->setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                        textComponent->setRenderMode(TextRenderMode::WORLD_SPACE);
                        textComponent->setAlignment(TextAlignment::CENTER);
                        textComponent->start(); // Initialize the text component
                    }
                }
                if (ImGui::MenuItem("Script Component")) {
                    if (!selected->getComponent<ScriptComponent>()) {
                        auto scriptComponent = selected->addComponent<ScriptComponent>();
                        scriptComponent->start();
                    }
                }
                ImGui::EndPopup();
            }
            
            ImGui::Separator();
            
            const auto& components = selected->getAllComponents();
            std::string componentToRemove;
            
            for (size_t i = 0; i < components.size(); i++) {
                const auto& component = components[i];
                if (component && component->isEnabled()) {
                    ImGui::PushID(static_cast<int>(i));
                    
                    std::string componentName = component->getTypeName();
                    std::string popupId = "ComponentContextMenu_" + std::to_string(i);
                    
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap;
                    bool treeOpen = ImGui::TreeNodeEx(componentName.c_str(), flags);
                    
                    if (ImGui::BeginPopupContextItem(popupId.c_str())) {
                        if (ImGui::MenuItem("Remove Component")) {
                            componentToRemove = componentName;
                        }
                        ImGui::EndPopup();
                    }
                    
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
                    if (ImGui::SmallButton("X")) {
                        componentToRemove = componentName;
                    }
                    ImGui::PopStyleColor(3);
                    
                    if (treeOpen) {
                        component->drawInspector();
                        ImGui::TreePop();
                    }
                    
                    ImGui::PopID();
                }
            }
            
            if (!componentToRemove.empty()) {
                if (componentToRemove == "CameraComponent") {
                    selected->removeComponent<CameraComponent>();
                } else if (componentToRemove == "MeshRenderer") {
                    selected->removeComponent<MeshRenderer>();
                } else if (componentToRemove == "ModelRenderer") {
                    selected->removeComponent<ModelRenderer>();
                } else if (componentToRemove == "AnimationComponent") {
                    selected->removeComponent<AnimationComponent>();
                } else if (componentToRemove == "LightComponent") {
                    selected->removeComponent<LightComponent>();
                } else if (componentToRemove == "PhysicsComponent") {
                    selected->removeComponent<PhysicsComponent>();
                } else if (componentToRemove == "Area3DComponent") {
                    selected->removeComponent<Area3DComponent>();
                } else if (componentToRemove == "TextComponent") {
                    selected->removeComponent<TextComponent>();
                } else if (componentToRemove == "ScriptComponent") {
                    selected->removeComponent<ScriptComponent>();
                }
            }
            
            // Show collision shape info if this is a collision-only node
            if (selected->getComponent<PhysicsComponent>() && !selected->getComponent<MeshRenderer>()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "⚡ Collision Shape Node");
                ImGui::TextWrapped("This node contains only collision physics. It will be invisible in the game but will participate in physics simulation.");
                ImGui::TextWrapped("You can move and scale this node to adjust the collision shape independently from the visual mesh.");
            }
            
            // Model Renderer properties
            auto modelRenderer = selected->getComponent<ModelRenderer>();
            if (modelRenderer) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "🎨 Model Renderer");
                
                // Model file path
                ImGui::Text("Model Path:");
                ImGui::SameLine();
                if (modelRenderer->isModelLoaded()) {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s", modelRenderer->getModelPath().c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "No model loaded");
                }
                
                if (ImGui::Button("Load Model...")) {
                    std::string modelPath = FileDialog::openFileDialog("Select 3D Model", "*.gltf");
                    if (!modelPath.empty()) {
                        if (modelRenderer->loadModel(modelPath)) {
                            std::cout << "Successfully loaded model: " << modelPath << std::endl;
                        } else {
                            std::cout << "Failed to load model: " << modelPath << std::endl;
                        }
                    }
                }
                
                if (modelRenderer->isModelLoaded()) {
                    ImGui::Separator();
                    ImGui::Text("Model Info:");
                    
                    auto meshes = modelRenderer->getMeshes();
                    auto materials = modelRenderer->getMaterials();
                    
                    ImGui::Text("Meshes: %zu", meshes.size());
                    ImGui::Text("Materials: %zu", materials.size());
                    
                    if (!materials.empty()) {
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "📁 Loaded Textures:");
                        
                        for (size_t i = 0; i < materials.size(); ++i) {
                            auto material = materials[i];
                            if (material) {
                                ImGui::PushID(static_cast<int>(i));
                                
                                if (ImGui::CollapsingHeader(("Material " + std::to_string(i)).c_str())) {
                                    auto diffuseTexture = material->getDiffuseTexture();
                                    if (diffuseTexture) {
                                        ImGui::Text("Diffuse: %s", diffuseTexture->getFilePath().c_str());
                                    } else {
                                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Diffuse: None");
                                    }
                                    
                                    // Normal texture
                                    auto normalTexture = material->getNormalTexture();
                                    if (normalTexture) {
                                        ImGui::Text("Normal: %s", normalTexture->getFilePath().c_str());
                                    } else {
                                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Normal: None");
                                    }
                                    
                                    auto armTexture = material->getARMTexture();
                                    if (armTexture) {
                                        ImGui::Text("ARM: %s", armTexture->getFilePath().c_str());
                                    } else {
                                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "ARM: None");
                                    }
                                }
                                
                                ImGui::PopID();
                            }
                        }
                    }
                }
            }
        }
        
    } else {
        ImGui::Text("No node selected");
    }
    
    ImGui::End();
}

void EditorUI::renderViewport() {
    ImGui::Begin("Viewport", &showViewport);
    
    bool isViewportFocused = ImGui::IsWindowFocused();
    editor.setViewportFocused(isViewportFocused);
    
    static ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE currentMode = ImGuizmo::LOCAL;
    
    auto cameraNode = editor.getActiveCamera();
    auto selectedNode = editor.getSelectedNode();
    
    if (cameraNode && selectedNode && isViewportFocused) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
        ImGui::PushStyleColor(ImGuiCol_Button, currentOperation == ImGuizmo::TRANSLATE ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Translate")) {
            currentOperation = ImGuizmo::TRANSLATE;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, currentOperation == ImGuizmo::ROTATE ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Rotate")) {
            currentOperation = ImGuizmo::ROTATE;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, currentOperation == ImGuizmo::SCALE ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Scale")) {
            currentOperation = ImGuizmo::SCALE;
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::Separator();
    }
    
    renderCameraControls();
    
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    
    const float VITA_ASPECT_RATIO = 960.0f / 544.0f;
    
    ImVec2 viewportSize;
    if (availableSize.x / availableSize.y > VITA_ASPECT_RATIO) {
        viewportSize.y = availableSize.y;
        viewportSize.x = availableSize.y * VITA_ASPECT_RATIO;
    } else {
        viewportSize.x = availableSize.x;
        viewportSize.y = availableSize.x / VITA_ASPECT_RATIO;
    }
    
    ImVec2 viewportPos = ImGui::GetCursorPos();
    viewportPos.x += (availableSize.x - viewportSize.x) * 0.5f;
    viewportPos.y += (availableSize.y - viewportSize.y) * 0.5f;
    ImGui::SetCursorPos(viewportPos);
    
    if (viewportSize.x != editor.getViewportSize().x || viewportSize.y != editor.getViewportSize().y) {
        editor.setViewportSize(glm::vec2(viewportSize.x, viewportSize.y));
        
        auto& framebuffer = editor.getViewportFramebuffer();
        if (framebuffer) {
            framebuffer->resize((int)viewportSize.x, (int)viewportSize.y);
        }
    }
    
    auto& framebuffer = editor.getViewportFramebuffer();
    if (!framebuffer && viewportSize.x > 0 && viewportSize.y > 0) {
        framebuffer = std::unique_ptr<Framebuffer>(new Framebuffer());
        if (!framebuffer->create((int)viewportSize.x, (int)viewportSize.y)) {
            std::cerr << "Failed to create viewport framebuffer!" << std::endl;
            framebuffer.reset();
        }
    }
    
    auto scene = editor.getActiveScene();
    if (scene && viewportSize.x > 0 && viewportSize.y > 0) {
        editor.renderSceneToViewport();
        
        ImGui::SetCursorPos(viewportPos);
        
        if (framebuffer) {
            ImGui::Image(
                (void*)(intptr_t)framebuffer->getColorTexture(),
                viewportSize,
                ImVec2(0, 1),
                ImVec2(1, 0)
            );
        }
        
        ImVec2 imageScreenPos = ImGui::GetItemRectMin();
        ImVec2 imageScreenSize = ImVec2(ImGui::GetItemRectMax().x - imageScreenPos.x, ImGui::GetItemRectMax().y - imageScreenPos.y);
        
        if (!cameraNode) {
            ImGui::SetCursorPos(ImVec2(viewportPos.x + 10, viewportPos.y + 30));
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No active camera");
            ImGui::SetCursorPos(ImVec2(viewportPos.x + 10, viewportPos.y + 50));
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Create a camera to view the scene");
        } else {
            if (selectedNode && isViewportFocused) {
                auto cameraComponent = cameraNode->getComponent<CameraComponent>();
                if (cameraComponent) {
                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
                    ImGuizmo::SetRect(imageScreenPos.x, imageScreenPos.y, imageScreenSize.x, imageScreenSize.y);
                    
                    glm::mat4 viewMatrix = cameraComponent->getViewMatrix();
                    glm::mat4 projectionMatrix = cameraComponent->getProjectionMatrix();
                    
                    glm::mat4 worldMatrix = selectedNode->getWorldMatrix();
                    
                    float view[16];
                    float projection[16];
                    float matrix[16];
                    
                    memcpy(view, glm::value_ptr(viewMatrix), 16 * sizeof(float));
                    memcpy(projection, glm::value_ptr(projectionMatrix), 16 * sizeof(float));
                    memcpy(matrix, glm::value_ptr(worldMatrix), 16 * sizeof(float));
                    
                    bool manipulated = ImGuizmo::Manipulate(
                        view, projection,
                        currentOperation, currentMode,
                        matrix
                    );
                    
                    if (manipulated && isViewportFocused) {
                        glm::mat4 newWorldMatrix;
                        memcpy(glm::value_ptr(newWorldMatrix), matrix, 16 * sizeof(float));
                        
                        auto parent = selectedNode->getParent();
                        if (parent) {
                            glm::mat4 parentWorldMatrix = parent->getWorldMatrix();
                            glm::mat4 parentWorldInverse = glm::inverse(parentWorldMatrix);
                            glm::mat4 newLocalMatrix = parentWorldInverse * newWorldMatrix;
                            
                            float translation[3], rotation[3], scale[3];
                            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(newLocalMatrix), translation, rotation, scale);
                            
                            selectedNode->getTransform().setPosition(glm::vec3(translation[0], translation[1], translation[2]));
                            selectedNode->getTransform().setEulerAngles(glm::vec3(rotation[0], rotation[1], rotation[2]));
                            selectedNode->getTransform().setScale(glm::vec3(scale[0], scale[1], scale[2]));
                        } else {
                            float translation[3], rotation[3], scale[3];
                            ImGuizmo::DecomposeMatrixToComponents(matrix, translation, rotation, scale);
                            
                            selectedNode->getTransform().setPosition(glm::vec3(translation[0], translation[1], translation[2]));
                            selectedNode->getTransform().setEulerAngles(glm::vec3(rotation[0], rotation[1], rotation[2]));
                            selectedNode->getTransform().setScale(glm::vec3(scale[0], scale[1], scale[2]));
                        }
                    }
                }
            }
        }
        
        ImGui::SetCursorPos(ImVec2(viewportPos.x + 10, viewportPos.y + viewportSize.y - 40));
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Vita Aspect Ratio (16:9)");
        ImGui::SetCursorPos(ImVec2(viewportPos.x + 10, viewportPos.y + viewportSize.y - 20));
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Viewport: %dx%d", (int)viewportSize.x, (int)viewportSize.y);
    } else {
        ImGui::Text("Viewport (%dx%d)", (int)viewportSize.x, (int)viewportSize.y);
        ImGui::Text("No active scene or invalid viewport size");
    }
    
    ImGui::End();
}

void EditorUI::renderFileExplorer() {
    ImGui::Begin("File Explorer", &showFileExplorer);
    ImGui::Text("File explorer will be implemented here");
    ImGui::End();
}

void EditorUI::renderInputMapping() {
    if (!showInputMapping) return;
    
    ImGui::Begin("Input Mapping Configuration", &showInputMapping);
    
    auto& inputManager = GetEngine().getInputManager();
    auto& inputMapping = inputManager.getInputMapping();
    
    // Header with hot-reload status
    ImGui::Text("Input Action Configuration");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(Hot-reload enabled)");
    ImGui::Separator();
    
    // File operations section
    ImGui::Text("File Operations:");
    if (ImGui::Button("Reload from File")) {
        inputMapping.reloadMappings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save to File")) {
        inputMapping.saveMappingsToFile("input_mappings.txt");
        // Reload mappings to refresh the UI
        inputMapping.reloadMappings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open File in Editor")) {
#ifdef LINUX_BUILD
        system("xdg-open input_mappings.txt &");
#endif
    }
    
    ImGui::Separator();
    
    // Add new action section
    ImGui::Text("Add New Action:");
    static char newActionName[64] = "";
    ImGui::InputText("Action Name", newActionName, sizeof(newActionName));
    
    static int selectedPlatform = 0;
    const char* platformNames[] = {"PC", "VITA", "ALL"};
    ImGui::Combo("Platform", &selectedPlatform, platformNames, 3);
    
    static int selectedInputType = 0;
    const char* inputTypeNames[] = {"Keyboard Key", "Vita Button", "Analog Stick", "Mouse Button", "Mouse Axis"};
    ImGui::Combo("Input Type", &selectedInputType, inputTypeNames, 5);
    
    static int inputCode = 0;
    ImGui::InputInt("Input Code", &inputCode);
    
    static int selectedActionType = 1;
    const char* actionTypeNames[] = {"Pressed", "Held", "Released"};
    ImGui::Combo("Action Type", &selectedActionType, actionTypeNames, 3);
    
    if (ImGui::Button("Add Action")) {
        if (strlen(newActionName) > 0) {
            InputMapping mapping(newActionName, 
                               static_cast<InputType>(selectedInputType), 
                               inputCode, 
                               static_cast<InputActionType>(selectedActionType));
            inputMapping.addMapping(mapping);
            
            newActionName[0] = '\0';
            inputCode = 0;
        }
    }
    
    ImGui::Separator();
    
    const auto& mappings = inputMapping.getAllMappings();
    
    if (mappings.empty()) {
        ImGui::Text("No input mappings configured.");
        ImGui::Text("Use the 'Add Action' section above to create new mappings.");
    } else {
        ImGui::Text("Current Mappings (%zu total):", mappings.size());
        
        ImGui::Columns(6, "InputMappingColumns");
        ImGui::SetColumnWidth(0, 120);
        ImGui::SetColumnWidth(1, 80);
        ImGui::SetColumnWidth(2, 100);
        ImGui::SetColumnWidth(3, 80);
        ImGui::SetColumnWidth(4, 80);
        ImGui::SetColumnWidth(5, 100);
        
        ImGui::Text("Action Name"); ImGui::NextColumn();
        ImGui::Text("Platform"); ImGui::NextColumn();
        ImGui::Text("Input Type"); ImGui::NextColumn();
        ImGui::Text("Code"); ImGui::NextColumn();
        ImGui::Text("Action"); ImGui::NextColumn();
        ImGui::Text("Controls"); ImGui::NextColumn();
        ImGui::Separator();
        
        for (size_t i = 0; i < mappings.size(); ++i) {
            const auto& mapping = mappings[i];
            
            // Action name
            ImGui::Text("%s", mapping.actionName.c_str());
            ImGui::NextColumn();
            
            // Platform (inferred from input type)
            const char* platform = "Unknown";
            if (mapping.inputType == InputType::KEYBOARD_KEY || 
                mapping.inputType == InputType::MOUSE_BUTTON || 
                mapping.inputType == InputType::MOUSE_AXIS) {
                platform = "PC";
            } else if (mapping.inputType == InputType::VITA_BUTTON) {
                platform = "VITA";
            } else if (mapping.inputType == InputType::ANALOG_STICK) {
                platform = "Both";
            }
            ImGui::Text("%s", platform);
            ImGui::NextColumn();
            
            const char* typeNames[] = {"Keyboard", "Vita Button", "Analog Stick", "Mouse Button", "Mouse Axis"};
            ImGui::Text("%s", typeNames[static_cast<int>(mapping.inputType)]);
            ImGui::NextColumn();
            
            if (mapping.inputType == InputType::KEYBOARD_KEY) {
                const char* keyName = "Unknown";
#ifdef LINUX_BUILD
                switch (mapping.inputCode) {
                    case GLFW_KEY_W: keyName = "W"; break;
                    case GLFW_KEY_A: keyName = "A"; break;
                    case GLFW_KEY_S: keyName = "S"; break;
                    case GLFW_KEY_D: keyName = "D"; break;
                    case GLFW_KEY_SPACE: keyName = "Space"; break;
                    case GLFW_KEY_LEFT_SHIFT: keyName = "Shift"; break;
                    case GLFW_KEY_LEFT_CONTROL: keyName = "Ctrl"; break;
                    case GLFW_KEY_E: keyName = "E"; break;
                    case GLFW_KEY_ESCAPE: keyName = "Esc"; break;
                    default: keyName = "Key"; break;
                }
#endif
                ImGui::Text("%s", keyName);
            } else {
                ImGui::Text("%d", mapping.inputCode);
            }
            ImGui::NextColumn();
            
            const char* actionNames[] = {"Pressed", "Held", "Released"};
            ImGui::Text("%s", actionNames[static_cast<int>(mapping.actionType)]);
            ImGui::NextColumn();
            
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Button("Test")) {
                bool isActive = inputMapping.isActionHeld(mapping.actionName);
                if (isActive) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ACTIVE");
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                inputMapping.removeMapping(mapping.actionName);
            }
            ImGui::PopID();
            ImGui::NextColumn();
        }
        
        ImGui::Columns(1);
    }
    
    ImGui::Separator();
    
    // Preset operations
    ImGui::Text("Preset Operations:");
    if (ImGui::Button("Load Default Mappings")) {
        inputMapping.loadMappingsFromFile("default_input_mappings.txt", true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All Mappings")) {
        inputMapping.clearMappings();
    }
    
    ImGui::Separator();
    
    // Help section
    ImGui::Text("Help:");
    ImGui::BulletText("Hot-reload is enabled - changes to input_mappings.txt are automatically loaded");
    ImGui::BulletText("Use 'Open File in Editor' to edit mappings in a text editor");
    ImGui::BulletText("Input codes: W=87, A=65, S=83, D=68, Space=32, Shift=340, Ctrl=341, E=69, Esc=256");
    ImGui::BulletText("Vita buttons: Cross=16384, Circle=32768, Square=8192, Triangle=4096");
    
    ImGui::End();
}

void EditorUI::renderCameraControls() {
    // Safety check - only render if we have a valid scene
    if (!editor.getActiveScene()) {
        return;
    }
    
    // Position the camera controls in the top-right corner of the viewport
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 controlPos = ImVec2(windowPos.x + windowSize.x - 200, windowPos.y + 10);
    
    ImGui::SetNextWindowPos(controlPos);
    ImGui::SetNextWindowSize(ImVec2(190, 120));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                           ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
    
    ImGui::Begin("Camera Controls", nullptr, flags);
    
    // Camera mode selection
    ImGui::Text("Camera Mode:");
    ImGui::SameLine();
    
    auto& editorSystem = editor;
    auto currentMode = editorSystem.getCameraMode();
    
    if (ImGui::RadioButton("Editor Camera", currentMode == EditorSystem::CameraMode::EDITOR_CAMERA)) {
        editorSystem.setCameraMode(EditorSystem::CameraMode::EDITOR_CAMERA);
    }
    if (ImGui::RadioButton("Game Camera", currentMode == EditorSystem::CameraMode::GAME_CAMERA)) {
        editorSystem.setCameraMode(EditorSystem::CameraMode::GAME_CAMERA);
    }
    
    ImGui::Separator();
    
    // Show current camera info
    auto activeCamera = editorSystem.getActiveCamera();
    if (activeCamera) {
        ImGui::Text("Active: %s", activeCamera->getName().c_str());
        
        auto cameraComponent = activeCamera->getComponent<CameraComponent>();
        if (cameraComponent) {
            auto& transform = activeCamera->getTransform();
            auto pos = transform.getPosition();
            ImGui::Text("Position: %.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
            ImGui::Text("FOV: %.1f°", cameraComponent->getFOV());
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No camera available");
    }
    
    ImGui::Separator();
    
    // Show controls info
    if (currentMode == EditorSystem::CameraMode::EDITOR_CAMERA) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Editor Camera Controls:");
        ImGui::Text("WASD - Move");
        ImGui::Text("Space/Shift - Up/Down");
        ImGui::Text("Arrow Keys - Rotate");
        
        // Show if viewport is focused and camera controls are active
        bool viewportFocused = editor.isViewportFocused();
        bool textInputActive = ImGui::GetIO().WantTextInput;
        
        if (viewportFocused && !textInputActive) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "V Camera Active");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "X Click viewport to activate");
        }
    } else {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Game Camera Controls:");
        ImGui::Text("WASD - Move");
        ImGui::Text("Space/Shift - Up/Down");
        ImGui::Text("Right Mouse - Rotate");
    }
    
    ImGui::End();
}

} // namespace GameEngine

#endif // LINUX_BUILD
