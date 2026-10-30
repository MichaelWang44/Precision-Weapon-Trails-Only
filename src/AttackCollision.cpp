#include "AttackCollision.h"

#include "Offsets.h"
#include "PrecisionHandler.h"
#include "Settings.h"

AttackCollision::AttackCollision(RE::ActorHandle a_actorHandle, const CollisionDefinition& a_collisionDefinition) :
	actorHandle(a_actorHandle), nodeName(a_collisionDefinition.nodeName)
{
	lastUpdate = *g_durationOfApplicationRunTimeMS;

	if (auto actor = a_actorHandle.get()) {
		if (auto currentProcess = actor->GetActorRuntimeData().currentProcess) {
			auto cell = actor->GetParentCell();
			if (cell) {
				if (Add(a_collisionDefinition)) {
					bool bIsWeapon = a_collisionDefinition.nodeName == "WEAPON"sv || a_collisionDefinition.nodeName == "SHIELD"sv;
					if (bIsWeapon) {
						RE::InventoryEntryData* weaponItem = nullptr;
						RE::TESForm* equipment = nullptr;
						RE::TESObjectWEAP* equippedWeapon = nullptr;

						bool bIsBashing = false;

						bool bIsLeftHand = a_collisionDefinition.nodeName != "WEAPON"sv;

						if (bIsLeftHand) {
							weaponItem = currentProcess->middleHigh->leftHand;
						} else {
							weaponItem = currentProcess->middleHigh->rightHand;
						}

						if (weaponItem) {
							equipment = weaponItem->object;
							if (equipment) {
								equippedWeapon = equipment->As<RE::TESObjectWEAP>();
							}
						}

						if (!equipment) {
							bIsWeapon = false;
						}

						bIsBashing = actor->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBash;

						bool bShowTrail = Settings::bDisplayTrails && !a_collisionDefinition.bNoTrail && !bIsBashing;

						if (bShowTrail) {
							if (!equippedWeapon || (equippedWeapon && equippedWeapon->weaponData.animationType == RE::WEAPON_TYPE::kHandToHandMelee)) {
								bShowTrail = false;
							}
						}

						// get visual weapon length
						if (equippedWeapon) {
							PrecisionHandler::TryGetCachedWeaponMeshReach(actor.get(), equippedWeapon, visualWeaponLength);
						}

						if (attackCollisionNode->parent) {
							visualWeaponLength *= attackCollisionNode->parent->local.scale;
						}

						if (!visualWeaponLength) {
							if (attackCollisionNode->parent && attackCollisionNode->parent->children.size() > 0) {
								auto& weaponNode = attackCollisionNode->parent->children[0];
								if (weaponNode && weaponNode != attackCollisionNode) {
									visualWeaponLength = PrecisionHandler::GetWeaponMeshLength(weaponNode.get());
								}
							}
						}

						// add trail
						if (bShowTrail) {
							PrecisionHandler::GetSingleton()->_attackTrails.emplace_back(std::make_shared<AttackTrail>(attackCollisionNode.get(), actorHandle, cell, weaponItem, bIsLeftHand, Settings::bTrailUseAttackCollisionLength ? true : a_collisionDefinition.bTrailUseTrueLength, a_collisionDefinition.trailOverride, trailFallbackLength));
						}
					}

// Visual geometry was calculated by Add without creating physics shapes.

					ID = a_collisionDefinition.ID;
					bNoRecoil = a_collisionDefinition.bNoRecoil;
					damageMult = a_collisionDefinition.damageMult;
					groundShake = a_collisionDefinition.groundShake;

					lifetime = a_collisionDefinition.duration;
					if (a_collisionDefinition.duration) {
						if (lifetime == 0) {
							lifetime = Settings::fDefaultCollisionLifetime;
							bool bPowerAttack = false;
							if (auto& attackData = Actor_GetAttackData(actor.get())) {
								bPowerAttack = attackData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack);
							}
							if (bPowerAttack) {
								*lifetime *= Settings::fDefaultCollisionLifetimePowerAttackMult;
							}
						}

						float weaponSpeedMult = 1.f;
						actor->GetGraphVariableFloat("weaponSpeedMult"sv, weaponSpeedMult);
						if (weaponSpeedMult == 0.f) {
							weaponSpeedMult = 1.f;
						}
						*lifetime *= (1.f / weaponSpeedMult);

						// really hacky fix for a weird issue
						if (*g_deltaTime > 0.011f) {
							*lifetime += *g_deltaTime;
						}

						if (a_collisionDefinition.durationMult) {
							*lifetime *= *a_collisionDefinition.durationMult;
						}
					}
				}
			}
		}
	}
}

AttackCollision::~AttackCollision()
{
	Remove();
}

bool AttackCollision::Add(const CollisionDefinition& a_collisionDefinition)
{
	auto actor = actorHandle.get().get();
    if (a_collisionDefinition.bNoTrail || !Settings::bDisplayTrails ||
        (nodeName != "WEAPON" && nodeName != "SHIELD")) { return false; }
	if (!actor) {
		return false;
	}

	auto cell = actor->GetParentCell();
	if (!cell) {
		return false;
	}



	auto root = actor->Get3D();
	if (!root) {
		return false;
	}

	auto bone = root->GetObjectByName(nodeName);
	if (!bone) {
		return false;
	}

	auto node = bone->AsNode();
	if (!node) {
		return false;
	}


    auto process = actor->GetActorRuntimeData().currentProcess;
    if (!process || !process->middleHigh || actor->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBash) { return false; }
    auto item = nodeName == "WEAPON" ? process->middleHigh->rightHand : process->middleHigh->leftHand;
    auto weapon = item && item->object ? item->object->As<RE::TESObjectWEAP>() : nullptr;
    if (!weapon || weapon->weaponData.animationType == RE::WEAPON_TYPE::kHandToHandMelee) { return false; }
    visualRoot.reset(root);
    visualCell = cell;
	float length = 0.f;
	float radius = 0.f;
	RE::hkVector4 vertexA{};
	RE::hkVector4 vertexB{};

	float havokWorldScale = *g_worldScale;
	float havokInvWorldScale = *g_worldScaleInverse;

	bool bIsRightWeapon = nodeName == "WEAPON"sv;
	bool bIsLeftWeapon = nodeName == "SHIELD"sv;

	RE::NiPoint3 tipOffset;

	bool bIsShieldBashing = false;
	bool bIsBowBashing = false;
	bool bIsBashing = actor->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBash;

	if (bIsRightWeapon || bIsLeftWeapon) {
		RE::TESObjectWEAP* currentWeapon = nullptr;

		if (!bIsBashing) {
			if (auto& attackData = Actor_GetAttackData(actor)) {
				bIsBashing = attackData->event == "bashStart"sv;
			}
		}

		auto currentProcess = actor->GetActorRuntimeData().currentProcess;

		if (bIsBashing) {
			auto rightHandEquipment = currentProcess->GetEquippedRightHand();
			auto leftHandEquipment = currentProcess->GetEquippedLeftHand();

			bool bIsBashingWithLeftHand = leftHandEquipment && leftHandEquipment != rightHandEquipment;  // has something else in the left hand so use the left hand node
			if (!bIsBashingWithLeftHand && rightHandEquipment) {                                         // check if it's a bow, bows are held in the left hand even if they're technically right hand
				if (auto rightHandWeapon = rightHandEquipment->As<RE::TESObjectWEAP>()) {
					if (rightHandWeapon->weaponData.animationType == RE::WEAPON_TYPE::kBow) {
						bIsBashingWithLeftHand = true;
						bIsBowBashing = true;
					}
				}
			}

			if (bIsBashingWithLeftHand) {
				bIsShieldBashing = leftHandEquipment && leftHandEquipment->IsArmor();

				nodeName = "SHIELD"sv;

				bone = root->GetObjectByName(nodeName);
				if (!bone) {
					return false;
				}
				node = bone->AsNode();
				if (!node) {
					return false;
				}

				if (leftHandEquipment) {
					currentWeapon = leftHandEquipment->As<RE::TESObjectWEAP>();
				}
			} else {
				if (rightHandEquipment) {
					currentWeapon = rightHandEquipment->As<RE::TESObjectWEAP>();
				}
			}
		}

		RE::NiAVObject* weaponNode = nullptr;
		if (node->children.size() > 0 && node->children[0]) {
			weaponNode = node->children[0].get();
		}

		// sum up mults
		float lengthMult = 1.f;
		float radiusMult = 1.f;

		if (a_collisionDefinition.lengthMult) {
			lengthMult *= *a_collisionDefinition.lengthMult;
		}

		if (a_collisionDefinition.radiusMult) {
			radiusMult *= *a_collisionDefinition.radiusMult;
		}

		if (a_collisionDefinition.transform) {
			lengthMult *= a_collisionDefinition.transform->scale;
			radiusMult *= a_collisionDefinition.transform->scale;
		}

		if (!currentWeapon) {
			if (bIsRightWeapon) {
				if (auto rightHandEquipment = currentProcess->GetEquippedRightHand()) {
					currentWeapon = rightHandEquipment->As<RE::TESObjectWEAP>();
				}
			} else if (bIsLeftWeapon) {
				if (auto leftHandEquipment = currentProcess->GetEquippedLeftHand()) {
					currentWeapon = leftHandEquipment->As<RE::TESObjectWEAP>();
				}
			}
		}

		// calc length and radius
		length = PrecisionHandler::GetWeaponAttackLength(actorHandle, weaponNode, currentWeapon, a_collisionDefinition.capsuleLength, lengthMult) * havokWorldScale;
		radius = PrecisionHandler::GetWeaponAttackRadius(actorHandle, currentWeapon, a_collisionDefinition.capsuleRadius, radiusMult) * havokWorldScale;

		// special cases
		if (bIsShieldBashing || bIsBowBashing) {
			float halfLength = length * 0.5f;
			vertexA.quad.m128_f32[0] = halfLength;
			vertexB.quad.m128_f32[0] = -halfLength;
			if (bIsShieldBashing) {
				radius = length;
			}
		} else {
			vertexA.quad.m128_f32[0] = length;
		}

		if (a_collisionDefinition.bWeaponTip) {
			float offset = (length - radius) * havokInvWorldScale;
			tipOffset = { 0.f, offset, 0.f };
		}
	} else if (node) {
		// Set default fallback values based on hand collision's capsule
		radius = 0.068565f;
		vertexA = { 0.002939f, 0.003175f, 0.054681f, 0.f };
		vertexB = { 0.001875f, 0.003175f, 0.054681f, 0.f };

		// sum up mults
		float lengthMult = 1.f;
		float radiusMult = 1.f;

		if (a_collisionDefinition.lengthMult) {
			lengthMult *= *a_collisionDefinition.lengthMult;
		}

		if (a_collisionDefinition.radiusMult) {
			radiusMult *= *a_collisionDefinition.radiusMult;
		}

		if (a_collisionDefinition.transform) {
			radiusMult *= a_collisionDefinition.transform->scale;
			lengthMult *= a_collisionDefinition.transform->scale;
		}

		// calc attack dimensions
		if (!PrecisionHandler::GetNodeAttackDimensions(actorHandle, node, a_collisionDefinition.capsuleLength, lengthMult, a_collisionDefinition.capsuleRadius, radiusMult, vertexA, vertexB, radius)) {
			return false;
		}
	} else {
		return false;
	}

	auto attackNode = RE::NiNode::Create(0);
	node->AttachChild(attackNode, true);

	if (a_collisionDefinition.transform) {
		attackNode->local = *a_collisionDefinition.transform;
	}

	if (a_collisionDefinition.bWeaponTip) {
		attackNode->local.translate += tipOffset;
	}

	if (bIsRightWeapon || bIsLeftWeapon) {
		RE::NiMatrix3 weaponRotation(0.f, RE::NI_HALF_PI, -RE::NI_HALF_PI);
		RE::NiMatrix3 newRotation = weaponRotation * attackNode->local.rotate;
		attackNode->local.rotate = newRotation;
	}


    // Geometry only: no bhkCollisionObject, no rigid body and no world registration.
    attackCollisionNode = RE::NiPointer<RE::NiNode>(attackNode);
    const float axisLength = Utils::HkVectorToNiPoint(vertexA).GetDistance(Utils::HkVectorToNiPoint(vertexB)) * havokInvWorldScale;
    trailFallbackLength = fmax(axisLength, radius * havokInvWorldScale);
    capsuleLength = fmax(axisLength, radius * 2.f * havokInvWorldScale);

	return true;
}

bool AttackCollision::Remove()
{
	if (!attackCollisionNode && !recoilCollisionNode) {
		return false;
	}

	RemoveCollision(recoilCollisionNode);
	RemoveCollision(attackCollisionNode);

	recoilCollisionNode = nullptr;
	attackCollisionNode = nullptr;

	return true;
}

bool AttackCollision::RemoveRecoilCollision()
{
	if (!recoilCollisionNode) {
		return false;
	}

	RemoveCollision(recoilCollisionNode);

	recoilCollisionNode = nullptr;

	return true;
}

bool AttackCollision::IsValid() const
{
	if (attackCollisionNode) {
		return true;
	}

	return false;
}

float AttackCollision::GetVisualWeaponLength() const
{
	return visualWeaponLength;
}

bool AttackCollision::HasHitRef(RE::ObjectRefHandle a_handle) const
{
	if (!ID) {
		return _hitRefs.HasHitRef(a_handle);
	} else {
		return PrecisionHandler::GetSingleton()->HasIDHitRef(actorHandle, *ID, a_handle);
	}
}

void AttackCollision::AddHitRef(RE::ObjectRefHandle a_handle, float a_duration, bool a_bIsNPC)
{
	if (!ID) {
		_hitRefs.AddHitRef(a_handle, a_duration, a_bIsNPC);
	} else {
		PrecisionHandler::GetSingleton()->AddIDHitRef(actorHandle, *ID, a_handle, a_duration, a_bIsNPC);
	}
}

void AttackCollision::ClearHitRefs()
{
	if (!ID) {
		_hitRefs.ClearHitRefs();
	} else {
		PrecisionHandler::GetSingleton()->ClearIDHitRefs(actorHandle, *ID);
	}
}

void AttackCollision::IncreaseDamagedCount()
{
	if (!ID) {
		_hitRefs.IncreaseDamagedCount();
	} else {
		PrecisionHandler::GetSingleton()->IncreaseIDDamagedCount(actorHandle, *ID);
	}
}

bool AttackCollision::HasHitMaterial(RE::MATERIAL_ID a_materialID) const
{
	ReadLocker locker(hitMaterialsLock);

	return _hitMaterials.contains(a_materialID);
}

void AttackCollision::AddHitMaterial(RE::MATERIAL_ID a_materialID, float a_duration)
{
	WriteLocker locker(hitMaterialsLock);

	_hitMaterials.emplace(a_materialID, a_duration);
}

void AttackCollision::ClearHitMaterials()
{
	WriteLocker locker(hitMaterialsLock);

	_hitMaterials.clear();
}

uint32_t AttackCollision::GetHitCount() const
{
	if (!ID) {
		return _hitRefs.GetHitCount();
	} else {
		return PrecisionHandler::GetSingleton()->GetIDHitCount(actorHandle, *ID);
	}
}

uint32_t AttackCollision::GetHitNPCCount() const
{
	if (!ID) {
		return _hitRefs.GetHitNPCCount();
	} else {
		return PrecisionHandler::GetSingleton()->GetIDHitNPCCount(actorHandle, *ID);
	}
}

uint32_t AttackCollision::GetDamagedCount() const
{
	if (!ID) {
		return _hitRefs.GetDamagedCount();
	} else {
		return PrecisionHandler::GetSingleton()->GetIDDamagedCount(actorHandle, *ID);
	}
}

bool AttackCollision::Update(float a_deltaTime)
{
    if (lastUpdate == *g_durationOfApplicationRunTimeMS) { return true; }
    lastUpdate = *g_durationOfApplicationRunTimeMS;
    auto actor = actorHandle.get();
    if (!actor || actor->IsDead() || !actor->AsActorState()->IsWeaponDrawn() || actor->Get3D() != visualRoot.get() ||
        actor->GetParentCell() != visualCell || !attackCollisionNode || !attackCollisionNode->parent) { return false; }
    if (actor->IsPlayerRef()) { a_deltaTime *= Utils::GetPlayerTimeMultiplier(); }
    if (lifetime) {
        if (*lifetime <= 0.f) { return false; }
        *lifetime -= a_deltaTime;
    }
    return true;
}

bool AttackCollision::CreateCollision(RE::bhkWorld* a_world, RE::Actor* a_actor, RE::NiNode* a_parentNode, RE::NiNode* a_newNode, RE::hkVector4& a_vertexA, RE::hkVector4& a_vertexB, float a_radius, CollisionLayer a_collisionLayer)
{
    return false; // Hard-disabled: this build never creates Precision physics entities.
}

bool AttackCollision::RemoveCollision(RE::NiPointer<RE::NiNode>& a_node)
{
    if (!a_node) { return false; }
    if (auto parent = a_node->parent) { parent->DetachChild(a_node.get()); }
    return true;
}

void AttackCollisions::Update(float a_deltaTime)
{
	{
		WriteLocker locker(lock);

		for (auto it = _attackCollisions.begin(); it != _attackCollisions.end();) {
			auto& collision = *it;
			if (!collision->Update(a_deltaTime)) {
				it = _attackCollisions.erase(it);
				OnCollisionRemoved();
			} else {
				++it;
			}
		}
	}

	// update and remove expired entries
	for (auto it = _IDHitRefs.begin(); it != _IDHitRefs.end();) {
		it->second.Update(a_deltaTime);
		if (it->second.IsEmpty()) {
			it = _IDHitRefs.erase(it);
		} else {
			++it;
		}
	}
}

bool AttackCollisions::IsEmpty() const
{
	ReadLocker locker(lock);

	return _attackCollisions.empty();
}

std::shared_ptr<AttackCollision> AttackCollisions::GetAttackCollision(RE::NiAVObject* a_node) const
{
	ReadLocker locker(lock);

	auto it = std::find_if(_attackCollisions.begin(), _attackCollisions.end(), [a_node](auto& attackCollision) { return attackCollision->attackCollisionNode.get() == a_node; });
	if (it != _attackCollisions.end()) {
		return *it;
	}

	return nullptr;
}

std::shared_ptr<AttackCollision> AttackCollisions::GetAttackCollision(std::string_view a_nodeName) const
{
	ReadLocker locker(lock);

	auto it = std::find_if(_attackCollisions.begin(), _attackCollisions.end(), [a_nodeName](auto& attackCollision) { return attackCollision->nodeName == a_nodeName; });
	if (it != _attackCollisions.end()) {
		return *it;
	}

	return nullptr;
}

std::shared_ptr<AttackCollision> AttackCollisions::GetAttackCollisionFromRecoilNode(RE::NiAVObject* a_node) const
{
	ReadLocker locker(lock);

	auto it = std::find_if(_attackCollisions.begin(), _attackCollisions.end(), [a_node](auto& attackCollision) { return attackCollision->recoilCollisionNode.get() == a_node; });
	if (it != _attackCollisions.end()) {
		return *it;
	}

	return nullptr;
}

void AttackCollisions::AddAttackCollision(RE::ActorHandle a_actorHandle, const CollisionDefinition& a_collisionDefinition)
{
	auto newAttackCollision = std::make_shared<AttackCollision>(a_actorHandle, a_collisionDefinition);

	if (newAttackCollision->IsValid()) {
		WriteLocker locker(lock);

		_attackCollisions.emplace_back(newAttackCollision);
	}
}

bool AttackCollisions::RemoveRecoilCollision()
{
	ReadLocker locker(lock);

	for (auto& attackCollision : _attackCollisions) {
		if (attackCollision->RemoveRecoilCollision()) {
			return true;
		}
	}

	return false;
}

bool AttackCollisions::RemoveAttackCollision(const CollisionDefinition& a_collisionDefinition)
{
	WriteLocker locker(lock);

	auto prevSize = _attackCollisions.size();

	if (!_attackCollisions.empty()) {
		if (a_collisionDefinition.ID) {  // remove all matching ID
			_attackCollisions.erase(std::remove_if(_attackCollisions.begin(), _attackCollisions.end(), [a_collisionDefinition](auto& attackCollision) { return !attackCollision || attackCollision->ID == a_collisionDefinition.ID; }));
			OnCollisionRemoved();
		} else {  // remove the first matching the node name
			auto search = std::find_if(_attackCollisions.begin(), _attackCollisions.end(), [a_collisionDefinition](auto& attackCollision) { return !attackCollision || attackCollision->nodeName == a_collisionDefinition.nodeName; });
			if (search != _attackCollisions.end()) {
				_attackCollisions.erase(search);
				OnCollisionRemoved();
			} else {
				logger::error("Could not find an attack collision to remove: {}", a_collisionDefinition.nodeName);
			}
		}
	}

	return prevSize != _attackCollisions.size();
}

bool AttackCollisions::RemoveAttackCollision(std::shared_ptr<AttackCollision> a_attackCollision)
{
	WriteLocker locker(lock);

	auto prevSize = _attackCollisions.size();

	if (!_attackCollisions.empty()) {
		_attackCollisions.erase(std::remove_if(_attackCollisions.begin(), _attackCollisions.end(), [a_attackCollision](auto& attackCollision) { return attackCollision == a_attackCollision; }));
		OnCollisionRemoved();
	}

	return prevSize != _attackCollisions.size();
}

bool AttackCollisions::RemoveAllAttackCollisions()
{
	WriteLocker locker(lock);

	auto prevSize = _attackCollisions.size();

	_attackCollisions.clear();

	ClearData();

	return prevSize != _attackCollisions.size();
}

void AttackCollisions::OnCollisionRemoved()
{
	/*if (_attackCollisions.empty()) {
		ClearData();
	}*/
}

void AttackCollisions::ClearData()
{
	// reset the ignore flags
	ignoreVanillaAttackEvents = std::nullopt;
	bStartedWithWeaponSwing = false;
	bStartedWithWPNSwingUnarmed = false;

	_IDHitRefs.clear();
}

void AttackCollisions::ForEachAttackCollision(std::function<void(std::shared_ptr<AttackCollision>)> a_func) const
{
	ReadLocker locker(lock);

	for (auto& attackCollision : _attackCollisions) {
		a_func(attackCollision);
	}
}

bool AttackCollisions::HasHitRef(RE::ObjectRefHandle a_handle) const
{
	ReadLocker locker(lock);

	for (auto& attackCollision : _attackCollisions) {
		if (attackCollision->HasHitRef(a_handle)) {
			return true;
		}
	}

	return false;
}

uint32_t AttackCollisions::GetHitCount() const
{
	uint32_t hitCount = 0;
	for (auto& attackCollision : _attackCollisions) {
		hitCount += attackCollision->GetHitCount();
	}

	return hitCount;
}

uint32_t AttackCollisions::GetHitNPCCount() const
{
	uint32_t hitCount = 0;
	for (auto& attackCollision : _attackCollisions) {
		hitCount += attackCollision->GetHitNPCCount();
	}

	return hitCount;
}

bool AttackCollisions::HasIDHitRef(uint8_t a_ID, RE::ObjectRefHandle a_handle) const
{
	auto search = _IDHitRefs.find(a_ID);
	if (search != _IDHitRefs.end()) {
		return search->second.HasHitRef(a_handle);
	}

	return false;
}

void AttackCollisions::AddIDHitRef(uint8_t a_ID, RE::ObjectRefHandle a_handle, float a_duration, bool a_bIsNPC)
{
	_IDHitRefs[a_ID].AddHitRef(a_handle, a_duration, a_bIsNPC);
}

void AttackCollisions::ClearIDHitRefs(uint8_t a_ID)
{
	auto search = _IDHitRefs.find(a_ID);
	if (search != _IDHitRefs.end()) {
		return search->second.ClearHitRefs();
	}
}

void AttackCollisions::IncreaseIDDamagedCount(uint8_t a_ID)
{
	auto search = _IDHitRefs.find(a_ID);
	if (search != _IDHitRefs.end()) {
		return search->second.IncreaseDamagedCount();
	}
}

uint32_t AttackCollisions::GetIDHitCount(uint8_t a_ID) const
{
	auto search = _IDHitRefs.find(a_ID);
	if (search != _IDHitRefs.end()) {
		return search->second.GetHitCount();
	}

	return 0;
}

uint32_t AttackCollisions::GetIDHitNPCCount(uint8_t a_ID) const
{
	auto search = _IDHitRefs.find(a_ID);
	if (search != _IDHitRefs.end()) {
		return search->second.GetHitNPCCount();
	}

	return 0;
}

uint32_t AttackCollisions::GetIDDamagedCount(uint8_t a_ID) const
{
	auto search = _IDHitRefs.find(a_ID);
	if (search != _IDHitRefs.end()) {
		return search->second.GetDamagedCount();
	}

	return 0;
}
