#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace RE;

PlayerCharacter* p = nullptr;
BSScript::IVirtualMachine* vm = nullptr;
TESDataHandler* DH = nullptr;
BGSProjectile* cProj = nullptr;
ActorValueInfo* aPenet = nullptr;

EnchantmentItem* hopperENCH = nullptr;
BGSKeyword* WeaponTypeSniper = nullptr;

namespace temp
{
	struct RefrOrInventoryObj
	{
		TESObjectREFR* refr;
		TESObjectREFR* inv;
		uint16_t id;
	};

}

bool HasKeywordVM(BSScript::IVirtualMachine* vm, uint32_t i, TESForm* target, BGSKeyword* k)
{
	using func_t = decltype(&HasKeywordVM);
	REL::Relocation<func_t> func{ REL::ID(1400368) };
	return func(vm, i, target, k);
}

const char* GetModel(TESForm* send)
{
	using func_t = decltype(&GetModel);
	REL::Relocation<func_t> func{ REL::ID(1039522) };
	return func(send);
}

float GetRandomfloat(float a, float b)
{
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_real_distribution<float> dist(a, b);
	float randNum = dist(mt);

	return randNum;
}

void HitHead(std::monostate, Actor* a)
{
	//logger::info("헤드");
	if (!a)
		return;

	BSTSmartPointer<BipedAnim> tempanims = a->biped;
	if (!tempanims)
		return;

	if (!a->IsDead(true)) {
		//logger::info("안죽었어");
		if (!a->GetHostileToActor(p)) {
			//logger::info("아직 적대적인 적이 아님");
			return;
		}

		BSTArray<EquippedItem>* equipped = &p->currentProcess->middleHigh->equippedItems;
		if (!equipped)
			return;

		if (equipped->size() != 0 && (*equipped)[0].item.instanceData) {
			TESObjectWEAP* weap = (TESObjectWEAP*)(*equipped)[0].item.object;

			if (!weap)
				return;

			float vSpeed = weap->weaponData.rangedData->boltChargeSeconds;
			bool isBolt = vSpeed > 0.5;
			
			if (!isBolt) {
				float fPenet = p->GetActorValue(*aPenet);
				if (GetRandomfloat(0, 1) > 0.4 + (fPenet / 100)) {
					//logger::info("볼트액션이 아니거나 확률 계산에 실패");
					return;
				}
			}
		}
	}

	TESForm* armorForm = tempanims->object[3].parent.object;

	for (int i = 0; i <= 16; i += 16) {
		TESForm* form = tempanims->object[i].parent.object;
		if (form) {
			if (tempanims->object[i].skinned) {
				TBO_InstanceData* tempTBO = tempanims->object[i].parent.instanceData.get();
				if (!tempTBO)
					continue;

				if (armorForm) {
					if (armorForm == form)
						continue;
				}

				if (form->GetPlayable(tempTBO)) {
					//logger::info("{}번 슬롯은 플레이어블", i);
					a->UnequipArmorFromSlot((BIPED_OBJECT)i, true);

					const char* ModelPath = GetModel(form);
					if (!ModelPath || ModelPath[0] == '\0') {
						//logger::info("모델 경로 못구함");
					} else {
						NiAVObject* head = a->currentProcess->middleHigh->headNode;
						if (!head)
							return;

						TESModel* cModel = (TESModel*)cProj;
						cModel->model = ModelPath;

						cProj->data.speed = GetRandomfloat(430, 550);

						NiPoint3 pPoint = p->GetPosition();
						NiPoint3 aPoint = a->GetPosition();

						NiPoint3 headPoint = head->world.translate;

						double px = pPoint.x;
						double py = pPoint.y;
						double ax = aPoint.x;
						double ay = aPoint.y;

						double xDiff = ax - px;
						double yDiff = ay - py;
						double angleInRadians = -std::atan2(yDiff, xDiff) + (M_PI / 2.0) + GetRandomfloat(-0.3, 0.3);

						double heightDiff = pPoint.z - aPoint.z;
						double angleInZAxis = atan(heightDiff / pPoint.GetDistance(aPoint)) - GetRandomfloat(0.40, 0.75);
						if (angleInZAxis < -1.4) {
							angleInZAxis = -1.4;
						}

						NiPoint3 shotAngle(angleInZAxis, 0, angleInRadians);
						ObjectRefHandle tempproj = DH->CreateProjectileAtLocation(cProj, headPoint, shotAngle, a->parentCell, a->parentCell->worldSpace);
					}
					return;
				}
			}
		}
	}
}

void injectHeadEnchant(std::monostate)
{
	BSTArray<EquippedItem>* equipped = &p->currentProcess->middleHigh->equippedItems;
	if (!equipped)
		return;

	if (equipped->size() != 0 && (*equipped)[0].item.instanceData) {
		TESObjectWEAP* weapon = (TESObjectWEAP*)(*equipped)[0].item.object;

		if (!weapon)
			return;

		if (HasKeywordVM(vm, 0, weapon, WeaponTypeSniper)) {
			// 장착중인 무기에서 인스턴트 데이터 가져오는 코드
			TESObjectWEAP::InstanceData* instData = (TESObjectWEAP::InstanceData*)((*equipped)[0].item.instanceData).get();	

			if (!instData)
				return;

			// weaponData.enchantments가 nullptr일 경우 새로 할당하여 weaponData에 설정
			if (!instData->enchantments) {
				instData->enchantments = new BSTArray<EnchantmentItem*>();
			}

			// enchantments를 weaponData.enchantments로 설정
			BSTArray<EnchantmentItem*>* enchantments = instData->enchantments;

			// 이미 장착해있다면 장착하지 않음
			for (size_t i = 0; i < enchantments->size(); ++i) {
				if ((*enchantments)[i] == hopperENCH) {
					return;
				}
			}
			// enchantments에 추가
			enchantments->push_back(hopperENCH);
		}
	}
}

void OnF4SEMessage(F4SE::MessagingInterface::Message* msg)
{
	switch (msg->type) {
	case F4SE::MessagingInterface::kGameLoaded:
		DH = RE::TESDataHandler::GetSingleton();
		p = PlayerCharacter::GetSingleton();
		cProj = (BGSProjectile*)DH->LookupForm(0x800, "HP_HelmetPopper.esp");
		aPenet = (ActorValueInfo*)DH->LookupForm(0x00097341, "fallout4.esm");
		hopperENCH = (EnchantmentItem*)DH->LookupForm(0x80F, "HP_HelmetPopper.esp");
		WeaponTypeSniper = (BGSKeyword*)DH->LookupForm(0x01E325D, "Fallout4.esm");

		break;
	case F4SE::MessagingInterface::kPostLoadGame:
		injectHeadEnchant(std::monostate{});

		break;
	}
}

bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
{
	vm = a_vm;
	a_vm->BindNativeMethod("HP_HelmetPopper"sv, "HitHead"sv, HitHead);
	a_vm->BindNativeMethod("HP_HelmetPopper"sv, "injectHeadEnchant"sv, injectHeadEnchant);

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format("{}.log", Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("Global Log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::trace);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%^%l%$] %v"s);

	logger::info("{} v{}", Version::PROJECT, Version::NAME);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical("unsupported runtime v{}", ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	const F4SE::PapyrusInterface* papyrus = F4SE::GetPapyrusInterface();
	if (papyrus)
		papyrus->Register(RegisterPapyrusFunctions);

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	if (message)
		message->RegisterListener(OnF4SEMessage);

	return true;
}
