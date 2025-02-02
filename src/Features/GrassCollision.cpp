#include "GrassCollision.h"

#include "State.h"
#include "Util.h"
#include "VariableCache.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	GrassCollision::Settings,
	EnableGrassCollision)

void GrassCollision::DrawSettings()
{
	if (ImGui::TreeNodeEx("Grass Collision", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Grass Collision", (bool*)&settings.EnableGrassCollision);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Allows player and NPC collision to modify grass position.");
		}

		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text(std::format("Active/Total Actors : {}/{}", activeActorCount, totalActorCount).c_str());
		ImGui::Text(std::format("Total Capsules : {}", numCapsules).c_str());
		ImGui::TreePop();
	}
}

void GrassCollision::UpdateCollisions(PerFrame& perFrameData)
{
	std::vector<RE::Actor*> actorList{};

	// Actor query code from po3 under MIT
	// https://github.com/powerof3/PapyrusExtenderSSE/blob/7a73b47bc87331bec4e16f5f42f2dbc98b66c3a7/include/Papyrus/Functions/Faction.h#L24C7-L46
	if (const auto processLists = RE::ProcessLists::GetSingleton(); processLists) {
		std::vector<RE::BSTArray<RE::ActorHandle>*> actors;
		actors.push_back(&processLists->highActorHandles);  // High actors are in combat or doing something interesting
		for (auto array : actors) {
			for (auto& actorHandle : *array) {
				auto actorPtr = actorHandle.get();
				if (actorPtr && actorPtr.get() && actorPtr.get()->Is3DLoaded()) {
					actorList.push_back(actorPtr.get());
					totalActorCount++;
				}
			}
		}
	}

	if (auto player = RE::PlayerCharacter::GetSingleton())
		actorList.push_back(player);

	RE::NiPoint3 cameraPosition = Util::GetAverageEyePosition();

	auto eyePosition = Util::GetEyePosition(0);

	for (const auto actor : actorList) {
		if (numCapsules == 256)
			break;
		if (auto root = actor->Get3D(false)) {
			float distance = cameraPosition.GetDistance(actor->GetPosition()); 
			if (distance > 1024.0f)
				continue;

			activeActorCount++;
			RE::BSVisit::TraverseScenegraphObjects(root, [&](RE::NiAVObject* a_node) -> RE::BSVisit::BSVisitControl {
				if (numCapsules == 256)
					return RE::BSVisit::BSVisitControl::kStop;
				if (GetCapsuleParams(a_node, perFrameData.Capsules[numCapsules])) {
					// Ignore very small capsules
					if (perFrameData.Capsules[numCapsules].TopPosRadius.w < distance * 0.01f)
						return RE::BSVisit::BSVisitControl::kContinue;
					
					// The shader's world position has eye offset already applied
					perFrameData.Capsules[numCapsules].TopPosRadius.x -= eyePosition.x;
					perFrameData.Capsules[numCapsules].TopPosRadius.y -= eyePosition.y;
					perFrameData.Capsules[numCapsules].TopPosRadius.z -= eyePosition.z;

					perFrameData.Capsules[numCapsules].BottomPos.x -= eyePosition.x;
					perFrameData.Capsules[numCapsules].BottomPos.y -= eyePosition.y;
					perFrameData.Capsules[numCapsules].BottomPos.z -= eyePosition.z;

					numCapsules++;
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
		}
	}

	auto eyePositionDelta = eyePosition - Util::GetEyePosition(1);

	perFrameData.CapsuleInfo = float4(eyePositionDelta.x, eyePositionDelta.y, eyePositionDelta.z, (float)numCapsules);
}

void GrassCollision::Update()
{
	if (updatePerFrame) {
		PerFrame perFrameData{};

		perFrameData.CapsuleInfo.x = 0;

		numCapsules = 0;
		totalActorCount = 0;
		activeActorCount = 0;

		if (settings.EnableGrassCollision)
			UpdateCollisions(perFrameData);

		perFrame->Update(perFrameData);

		updatePerFrame = false;
	}

	auto& context = State::GetSingleton()->context;

	static Util::FrameChecker frameChecker;
	if (frameChecker.IsNewFrame()) {
		ID3D11Buffer* buffers[1];
		buffers[0] = perFrame->CB();
		context->VSSetConstantBuffers(5, ARRAYSIZE(buffers), buffers);
	}
}

void GrassCollision::LoadSettings(json& o_json)
{
	settings = o_json;
}

void GrassCollision::SaveSettings(json& o_json)
{
	o_json = settings;
}

void GrassCollision::RestoreDefaultSettings()
{
	settings = {};
}

void GrassCollision::PostPostLoad()
{
	Hooks::Install();
}

const RE::hkpCapsuleShape* GrassCollision::GetNodeCapsuleShape(RE::bhkNiCollisionObject* a_collisionObject)
{
	RE::bhkRigidBody* rigidBody = a_collisionObject->body.get() ? a_collisionObject->body.get()->AsBhkRigidBody() : nullptr;
	if (rigidBody && rigidBody->referencedObject) {
		RE::hkpRigidBody* hkpRigidBody = static_cast<RE::hkpRigidBody*>(rigidBody->referencedObject.get());
		const RE::hkpShape* hkpShape = hkpRigidBody->collidable.shape;
		if (hkpShape->type == RE::hkpShapeType::kCapsule) {
			auto hkpCapsuleShape = static_cast<const RE::hkpCapsuleShape*>(hkpShape);
			return hkpCapsuleShape;
		}
	}
	return nullptr;
}

bool GrassCollision::GetCapsuleParams(RE::NiAVObject* a_node, Capsule& a_outCapsule)
{
	if (a_node && a_node->collisionObject) {
		auto collisionObject = static_cast<RE::bhkCollisionObject*>(a_node->collisionObject.get());
		auto rigidBody = collisionObject->GetRigidBody();
		if (rigidBody && rigidBody->referencedObject) {
			RE::hkpRigidBody* hkpRigidBody = static_cast<RE::hkpRigidBody*>(rigidBody->referencedObject.get());
			const RE::hkpShape* hkpShape = hkpRigidBody->collidable.shape;
			if (hkpShape->type == RE::hkpShapeType::kCapsule) {
				auto hkpCapsuleShape = static_cast<const RE::hkpCapsuleShape*>(hkpShape);
				float bhkInvWorldScale = RE::bhkWorld::GetWorldScaleInverse();
				auto radius = hkpCapsuleShape->radius * bhkInvWorldScale;
				auto a = (Util::HkVectorToNiPoint(hkpCapsuleShape->vertexA) * bhkInvWorldScale);
				auto b = (Util::HkVectorToNiPoint(hkpCapsuleShape->vertexB) * bhkInvWorldScale);
				auto& hkTransform = hkpRigidBody->motion.motionState.transform;
				RE::NiTransform transform = Util::HkTransformToNiTransform(hkTransform);
				a = transform * a;
				b = transform * b;

				//a += a_node->world.translate;
				//b += a_node->world.translate;

				a_outCapsule.TopPosRadius = { a.x, a.y, a.z, radius };
				a_outCapsule.BottomPos = { b.x, b.y, b.z, 0.0f };
				return true;
			}
		}
	}
	return false;
}

void GrassCollision::SetupResources()
{
	perFrame = new ConstantBuffer(ConstantBufferDesc<PerFrame>());
}

void GrassCollision::Reset()
{
	updatePerFrame = true;
}

bool GrassCollision::HasShaderDefine(RE::BSShader::Type shaderType)
{
	switch (shaderType) {
	case RE::BSShader::Type::Grass:
		return true;
	default:
		return false;
	}
}

void GrassCollision::Hooks::BSGrassShader_SetupGeometry::thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags)
{
	VariableCache::GetSingleton()->grassCollision->Update();
	func(This, Pass, RenderFlags);
}
