#include <string>
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace RE;

PlayerCharacter* p = nullptr;
BSScript::IVirtualMachine* vm = nullptr;
TESDataHandler* DH = nullptr;
BGSProjectile* cProj = nullptr;

EnchantmentItem* hopperENCH = nullptr;
BGSKeyword* WeaponTypeSniper = nullptr;
BGSKeyword* helmetPA = nullptr;

TESRace* racePA = nullptr;

TESGlobal* Percent_Non_PA_BoltAction = nullptr;
TESGlobal* Percent_Non_PA_Non_BoltAction = nullptr;
TESGlobal* Percent_Non_PA_NonSniper = nullptr;

TESGlobal* Percent_PA_BoltAction = nullptr;
TESGlobal* Percent_PA_Non_BoltAction = nullptr;
TESGlobal* Percent_PA_NonSniper = nullptr;

TESGlobal* Power_PopHeadGear = nullptr;
TESGlobal* Power_PopHeadGear_Weapon = nullptr;
TESGlobal* Angle_PopHeadGear = nullptr;
TESGlobal* Pop_NonCollision = nullptr;

namespace temp
{
	struct RefrOrInventoryObj
	{
		TESObjectREFR* refr;
		TESObjectREFR* inv;
		uint16_t id;
	};

}

enum class WeaponType
{
	NonSniper,
	Sniper,
	BoltAction
};

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

	WeaponType weaponType = WeaponType::NonSniper; // 일단 무기의 기본값을 NonSniper로 설정

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

			//ㅡㅡㅡㅡㅡㅡㅡㅡㅡ 벗질지 말지 확률 계산 ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ
			float fRemovalChance = 0;

			// 파암을 입고 있나 확인
			bool isPA = false;
			if (p->race == racePA) {
				isPA = true;
			}
						
			if (HasKeywordVM(vm, 0, weap, WeaponTypeSniper)) {
				float vSpeed = weap->weaponData.rangedData->boltChargeSeconds;
				bool isBolt = vSpeed > 0.5;

				if (isBolt) {
					weaponType = WeaponType::BoltAction;  // 볼트액션일 경우
				} else {
					weaponType = WeaponType::Sniper; // 볼트액션이 아니면 일반 저격총
				}
			}

			// 파워아머 종족인지 확인하고 각 무기 타입에 따른 확률 설정
			switch (weaponType) {
			case WeaponType::NonSniper:
				{
					if (isPA) {
						fRemovalChance = Percent_PA_NonSniper->value;
					} else {
						fRemovalChance = Percent_Non_PA_NonSniper->value;
					}
					break;
				}
			case WeaponType::Sniper:
				{
					if (isPA) {
						fRemovalChance = Percent_PA_Non_BoltAction->value;
					} else {
						fRemovalChance = Percent_Non_PA_Non_BoltAction->value;
					}
					break;
				}
			case WeaponType::BoltAction:
				{
					if (isPA) {
						fRemovalChance = Percent_PA_BoltAction->value;
					} else {
						fRemovalChance = Percent_Non_PA_BoltAction->value;
					}
					break;
				}
			}

			if (fRemovalChance > 100) {
				fRemovalChance = 100;
			} else if (fRemovalChance < 0) {
				fRemovalChance = 0;
			}

			if (GetRandomfloat(0, 100) > fRemovalChance) {
				return; // 확률 계산에 실패했으므로 리턴함
			}
		}
	}

	TESForm* armorForm = tempanims->object[3].parent.object; // 33번 바디 슬롯의 장비를 구함

	for (int i = 0; i <= 16; i += 16) { // 30번이나 46번, 머리나 해드밴드 슬롯
		TESForm* form = tempanims->object[i].parent.object;
		if (form) {
			if (armorForm) {
				if (armorForm == form) {
					continue; // 머리장비지만 몸 슬롯도 차지하면 무시함
				}
			}

			if (((form->formFlags >> 2) & 1) == 1) {
				return; // 갑옷 플래그가 Non-Playable 임
			}

			const char* ModelPath = GetModel(form);
			if (!ModelPath || ModelPath[0] == '\0') {
				//logger::info("모델 경로 못구함");
				return;
			}

			NiAVObject* head = a->currentProcess->middleHigh->headNode;
			if (!head) {
				return; // 머리가 없어서 연산이 불가능
			}

			TESModel* cModel = (TESModel*)cProj;
			cModel->model = ModelPath; // 방어구의 모델 경로를 복사해 esp내의 더미 지뢰에 넣음

			//튕겨나가는 속도 글로벌 가져오기
			float popPower = Power_PopHeadGear->value;
			if (popPower < 260) {
				popPower = 260;
			} else if (popPower > 10000) {
				popPower = 10000;
			}

			//무기의 종류에 따라 튕겨나가는 힘에 차이를 둠
			if (Power_PopHeadGear_Weapon->value == 1) {
				switch (weaponType) {
				case WeaponType::Sniper:
					{
						popPower *= 1.15;
						break;
					}
				case WeaponType::BoltAction:
					{
						popPower *= 1.3;
						break;
					}
				}
			}

			// 튕겨나가는 속도에 편차를 줌
			float popSpread = popPower * 0.10;
			cProj->data.speed = GetRandomfloat(popPower - popSpread, popPower + popSpread);

			NiPoint3 pPoint = p->GetPosition();
			NiPoint3 aPoint = a->GetPosition();

			NiPoint3 headPoint = head->world.translate;

			double px = pPoint.x;
			double py = pPoint.y;
			double ax = aPoint.x;
			double ay = aPoint.y;

			// 튕겨나가는 각도 좌우 계산
			double xDiff = ax - px;
			double yDiff = ay - py;
			double angleInRadians = -std::atan2(yDiff, xDiff) + (M_PI / 2.0) + GetRandomfloat(-0.3, 0.3);

			// 튕겨나가는 위아래 각도 계산
			double heightDiff = pPoint.z - aPoint.z;
			float sendPopAngle = Angle_PopHeadGear->value * M_PI / 180;
			float popAngle = GetRandomfloat(sendPopAngle - 0.15, sendPopAngle + 0.15);
			double angleInZAxis = atan(heightDiff / pPoint.GetDistance(aPoint)) - popAngle;

			if (angleInZAxis < -1.4) {
				angleInZAxis = -1.4;
			}

			NiPoint3 shotAngle(angleInZAxis, 0, angleInRadians);
			ObjectRefHandle tempproj = DH->CreateProjectileAtLocation(cProj, headPoint, shotAngle, a->parentCell, a->parentCell->worldSpace);

			TESObjectREFR* fakeHelmRef = tempproj.get().get();
			if (!fakeHelmRef)
				return;

			fakeHelmRef->Load3D(false); // 콜리전 적용여부를 확인하기 위해 동기 방식으로 3D 모델을 로드시킴
			NiAVObject* tempMesh = tempproj.get().get()->Get3D();

			if (tempMesh) {
				if (!(tempMesh->collisionObject)) {
					fakeHelmRef->Disable();
					if (Pop_NonCollision->value == 0) {
						return;
					}
				}
			}

			a->UnequipArmorFromSlot((BIPED_OBJECT)i, true);
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

		if (!weapon || weapon->weaponData.ammo == nullptr)
			return; // 무기가 없거나 투사 무기가 아니면 리턴

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

void OnF4SEMessage(F4SE::MessagingInterface::Message* msg)
{
	switch (msg->type) {
	case F4SE::MessagingInterface::kGameLoaded:
		DH = RE::TESDataHandler::GetSingleton();
		p = PlayerCharacter::GetSingleton();
		cProj = (BGSProjectile*)DH->LookupForm(0x800, "HP_HelmetPopper.esp");
		hopperENCH = (EnchantmentItem*)DH->LookupForm(0x80F, "HP_HelmetPopper.esp");
		WeaponTypeSniper = (BGSKeyword*)DH->LookupForm(0x01E325D, "Fallout4.esm");
		helmetPA = (BGSKeyword*)DH->LookupForm(0x00182CD5, "Fallout4.esm");
		racePA = (TESRace*)DH->LookupForm(0x0001D31E, "Fallout4.esm");

		Percent_Non_PA_BoltAction = (TESGlobal*)DH->LookupForm(0x812, "HP_HelmetPopper.esp");
		Percent_Non_PA_Non_BoltAction = (TESGlobal*)DH->LookupForm(0x813, "HP_HelmetPopper.esp");
		Percent_Non_PA_NonSniper = (TESGlobal*)DH->LookupForm(0x814, "HP_HelmetPopper.esp");

		Percent_PA_BoltAction = (TESGlobal*)DH->LookupForm(0x815, "HP_HelmetPopper.esp");
		Percent_PA_Non_BoltAction = (TESGlobal*)DH->LookupForm(0x816, "HP_HelmetPopper.esp");
		Percent_PA_NonSniper = (TESGlobal*)DH->LookupForm(0x817, "HP_HelmetPopper.esp");

		Power_PopHeadGear = (TESGlobal*)DH->LookupForm(0x818, "HP_HelmetPopper.esp");
		Power_PopHeadGear_Weapon = (TESGlobal*)DH->LookupForm(0x81a, "HP_HelmetPopper.esp");
		Angle_PopHeadGear = (TESGlobal*)DH->LookupForm(0x81b, "HP_HelmetPopper.esp");
		Pop_NonCollision = (TESGlobal*)DH->LookupForm(0x81c, "HP_HelmetPopper.esp");

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
