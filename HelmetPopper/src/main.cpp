#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <windows.h>
#include <filesystem>

#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif

using namespace RE;
namespace fs = std::filesystem;

namespace temp
{
	struct RefrOrInventoryObj
	{
		TESObjectREFR* refr;
		TESObjectREFR* inv;
		uint16_t id;
	};
}

struct FormOrInventoryObj
{
	TESForm* form{ nullptr };  // TESForm 포인터를 가리키는 포인터
	uint64_t second_arg{ 0 };  // unsigned 64비트 정수
};

enum class WeaponType
{
	NonSniper,
	Sniper,
	BoltAction
};

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
//TESGlobal* Pop_NonCollision = nullptr;
TESGlobal* Pop_Force_NonSniper = nullptr;

UI* ui = nullptr;
std::vector<ObjectRefHandle> helmetHandles;
const size_t MAX_HELMET_HANDLES = 20;

std::string lootDir;
std::vector<TESForm*> skipList;
TESObjectREFR* filterBox = nullptr;

WeaponType weaponType;

bool isRunning = false;

bool AddItemVM(BSScript::IVirtualMachine* vm, uint32_t i, TESObjectREFR* target, FormOrInventoryObj obj, uint32_t count, bool b1)
{
	using func_t = decltype(&AddItemVM);
	REL::Relocation<func_t> func{ REL::ID(1212351) };
	return func(vm, i, target, obj, count, b1);
}

bool RemoveItemVM(BSScript::IVirtualMachine* vm, uint32_t i, TESObjectREFR* target, FormOrInventoryObj obj, uint32_t count, bool b1, TESObjectREFR* sender)
{
	using func_t = decltype(&RemoveItemVM);
	REL::Relocation<func_t> func{ REL::ID(492460) };
	return func(vm, i, target, obj, count, b1, sender);
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

TESForm* getFormformFile(std::string tempLine)
{
	TESForm* form = nullptr;

	const int MIN_FORM_ID = 0;
	const int MAX_FORM_ID = 0xFFFFFF;  // 예시 범위
	size_t pos = tempLine.find("**");
	if (pos == std::string::npos) {
		return form;  // 문자열에 **가 없음
	}

	std::istringstream iss(tempLine);

	std::string espName = tempLine.substr(0, pos);  // ** 이전 부분
	std::string stringFormID = tempLine.substr(pos + 2);
	int formID;
	std::istringstream(stringFormID) >> std::hex >> formID;

	if (formID < MIN_FORM_ID || formID > MAX_FORM_ID) {
		return form;  // 받아온 값이 이상함
	}

	form = DH->LookupForm(formID, espName);

	return form;
}

void FillContainerfromFile(std::monostate)  // 필터 가구를 열때 txt 목록 아이템을 상자에 넣음
{
	std::string skipName = "skipFilter.txt";
	std::string skipPath = lootDir + skipName;
	std::ifstream SkipFile(skipPath);  // 필터가 저장된 txt 파일을 불러옴
	if (!SkipFile) {
		return;  // 파일을 열지 못한 경우
	}

	//logger::info("파일 경로: {}", skipPath);

	std::string tempLine02;
	while (std::getline(SkipFile, tempLine02)) {
		if (tempLine02.empty()) {
			continue;
		}

		TESForm* form = getFormformFile(tempLine02);
		if (form) {
			FormOrInventoryObj tempObj;
			tempObj.form = form;
			AddItemVM(vm, 0, filterBox, tempObj, 1, true);
		}
	}

	SkipFile.close();

	return;
}

bool loadFilterSettingsFromFiles()  // esp에 적은 필터와 txt 필터를 배열에 삽입
{
	skipList.clear();

	std::string skipName = "skipFilter.txt";
	std::string skipPath = lootDir + skipName;
	std::ifstream SkipFile(skipPath);  // 필터가 저장된 txt 파일을 불러옴
	if (!SkipFile) {
		return false;  // 파일을 열지 못한 경우
	}

	std::string line;
	std::vector<std::string> lines;
	while (std::getline(SkipFile, line)) {
		lines.push_back(line);  // 파일 내용 전체를 메모리에 저장
	}
	SkipFile.close();

	// skipList에 필터 추가
	std::vector<std::string> validLines;
	for (const auto& tempLine02 : lines) {
		if (tempLine02.empty()) {
			continue;
		}

		TESForm* form = getFormformFile(tempLine02);

		if (form) {
			stl::enumeration<ENUM_FORM_ID, std::uint8_t> type = form->formType;
			if (type == ENUM_FORM_ID::kARMO) {
				TESObjectARMO* armor = (TESObjectARMO*)form;
				if (armor) {
					auto bipedFlag = armor->bipedModelData.bipedObjectSlots;
					if ((bipedFlag & (1 << 0)) != 0 || (bipedFlag & (1 << 16)) != 0) {
						skipList.push_back(form);
						validLines.push_back(tempLine02);  // 조건에 맞는 항목만 저장
					}
				}
			}
		}
	}

	// 필터 조건에 맞는 항목들만 원본 파일에 덮어쓰기
	std::ofstream SkipFileOut(skipPath);  // 파일을 다시 엽니다
	if (!SkipFileOut) {
		return false;
	}

	for (const auto& validLine : validLines) {
		SkipFileOut << validLine << '\n';  // 조건에 맞는 항목만 기록
	}

	SkipFileOut.close();
	return true;
}

bool setSkipFilter(std::monostate)  // 루팅 필터 가구를 닫을때 실행됨
{
	BGSInventoryList* temp = filterBox->inventoryList;  // 필터 상자의 인벤토리 리스트 가져오기
	if (!temp) {
		return false;
	}

	std::string fileName = "skipFilter.txt";            // 저장할 파일 이름 설정
	std::string filePath = lootDir + fileName;          // 파일 경로 설정
	std::ofstream fileStream(filePath, std::ios::out);  // 파일을 쓰기 모드로 열기

	if (!fileStream.is_open()) {
		//logger::error("파일을 열지 못했습니다.");
		return false;  // 파일을 열지 못한 경우 종료
	}
	//logger::info("파일을 성공적으로 열었습니다: {}", filePath);


	std::vector<TESObjectREFR::RemoveItemData*> itemsToRemove;  // 제거할 아이템 목록 저장용 벡터

	// 상자 내의 아이템에서 아이템 분류하고 메모장에 적기
	BSTArray<BGSInventoryItem> boxList = temp->data;
	if (!boxList.empty()) {
		for (BGSInventoryItem bItem : boxList) {
			TESBoundObject* obj = bItem.object;

			TESObjectARMO* armor = (TESObjectARMO*)obj;
			if (!armor) {
				//logger::info("null 체크에 걸림");
				continue;  // nullptr 체크
			}

			stl::enumeration<ENUM_FORM_ID, std::uint8_t> type = obj->formType;
			auto bipedFlag = armor->bipedModelData.bipedObjectSlots;

			if ((obj->formType != ENUM_FORM_ID::kARMO) || (bipedFlag & (1 << 0)) == 0 && (bipedFlag & (1 << 16)) == 0) {
				TESBoundObject* obj = bItem.object;
				uint32_t iCount = bItem.GetCount();

				FormOrInventoryObj tempObj;
				tempObj.form = obj;
				RemoveItemVM(vm, 0, filterBox, tempObj, iCount, true, p);

				continue;  // 상자에 들은 아이템이 방어구가 아니거나 30 46 플래그 없으면 아이템을 반환하고 다음으로
			}

			// 멤버함수 썼더니 최종수정 모드가 뽑혀서 배열로 원본 esp 검색
			BSStaticArray<TESFile*> overwriteArray = obj->sourceFiles.array[0];
			TESFile* espFile = overwriteArray[0];
			bool bEsl = false;
			if (espFile->IsLight()) {
				bEsl = true;
			}

			std::string_view nameTemp = espFile->GetFilename();
			std::string espName = std::string(nameTemp);

			std::stringstream ss;
			ss << std::hex << obj->formID;
			std::string hexString = ss.str();

			// esl 플래그 확인하고 폼id 저장
			if (bEsl) {
				if (hexString.size() < 3) {
					hexString = std::string(3 - hexString.size(), '0') + hexString;
				}
				hexString = hexString.substr(hexString.size() - 3, 3);
			} else {
				if (hexString.size() < 6) {
					hexString = std::string(6 - hexString.size(), '0') + hexString;
				}
				hexString = hexString.substr(hexString.size() - 6, 6);
			}

			std::string slash = "**";
			std::string text = std::string(espName) + slash + hexString;
			fileStream << text << std::endl;

			TESObjectREFR::RemoveItemData* rData = new TESObjectREFR::RemoveItemData(obj, 1);
			rData->reason = ITEM_REMOVE_REASON::kNone;
			itemsToRemove.push_back(rData);
		}
	}

	// 삭제시 배열 꼬임을 막기 위해 임시 배열에 저장하고 한번에 삭제
	if (!itemsToRemove.empty()) {
		for (TESObjectREFR::RemoveItemData* rData : itemsToRemove) {
			ObjectRefHandle dropRef = filterBox->RemoveItem(*rData);
			delete rData;  // 메모리 해제
		}
		itemsToRemove.clear();  // 벡터 초기화
	}

	fileStream.flush();  // 버퍼를 비워서 파일에 쓰기 작업을 완료합니다.
	fileStream.close();  // 파일을 닫습니다.
	//logger::info("파일을 성공적으로 닫았습니다: {}", filePath);

	if (loadFilterSettingsFromFiles()) {  // 기본 필터와 txt 필터를 각 배열에 삽입
		return true;
	}

	return false;
}


void HitHead(std::monostate, Actor* a)
{
	if (isRunning) {
		return;
	}

	isRunning = true;

	//logger::info("헤드");
	if (!a) {
		isRunning = false;
		return;
	}

	BSTSmartPointer<BipedAnim> tempanims = a->biped;
	if (!tempanims) {
		isRunning = false;
		return;
	}

	if (!a->IsDead(true) || (weaponType == WeaponType::NonSniper && Pop_Force_NonSniper->value == 0)) {
		//logger::info("안죽었거나 저격총이 아니고 mcm에서 옵션을 끔");
		if (!a->GetHostileToActor(p)) {
			//logger::info("아직 적대적인 적이 아님");
			isRunning = false;
			return;
		}

		//ㅡㅡㅡㅡㅡㅡㅡㅡㅡ 벗질지 말지 확률 계산 ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ
		float fRemovalChance = 0;

		// 파암을 입고 있나 확인
		bool isPA = false;
		if (a->race == racePA) {
			isPA = true;
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
			isRunning = false;
			return;  // 확률 계산에 실패했으므로 리턴함
		}
	}

	TESForm* armorForm = tempanims->object[3].parent.object;  // 33번 바디 슬롯의 장비를 구함
	TESForm* armorForm02 = tempanims->object[6].parent.object;  // 36번 바디 슬롯의 장비를 구함

	for (int i = 0; i <= 16; i += 16) {  // 30번이나 46번, 머리나 해드밴드 슬롯
		TESForm* form = tempanims->object[i].parent.object;
		if (form) {
			if (armorForm) {
				if (armorForm == form) {
					continue;  // 머리장비지만 몸 슬롯도 차지하면 무시함
				}
			}

			if (armorForm02) {
				if (armorForm02 == form) {
					continue;  // 머리장비지만 상체갑옷 슬롯도 차지하면 무시함
				}
			}

			if (((form->formFlags >> 2) & 1) == 1) {
				continue;  // 갑옷 플래그가 Non-Playable 임
			}

			bool existForm = false;
			for (TESForm* filterHelmet : skipList) {
				if (filterHelmet == form) {
					existForm = true; //  MCM에서 저장한 txt파일에 옮겨놓은 필터배열과 방어구를 비교
					break; 
				}
			}

			if (existForm) {
				continue;  // 필터배열과 같은게 있으면 다음으로
			}

			const char* ModelPath = GetModel(form);
			if (!ModelPath || ModelPath[0] == '\0') {
				//logger::info("모델 경로 못구함");
				continue;
			}

			NiAVObject* head = a->currentProcess->middleHigh->headNode;
			if (!head) {
				continue;  // 머리가 없어서 연산이 불가능
			}

			TESModel* cModel = (TESModel*)cProj;
			cModel->model = ModelPath;  // 방어구의 모델 경로를 복사해 esp내의 더미 지뢰에 넣음

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

			helmetHandles.push_back(tempproj);  // VATS 타겟이 되서 그냥 VATS켜면 없애버리게 하려고 배열에 저장

			if (helmetHandles.size() > MAX_HELMET_HANDLES) {  // 핸들이 너무 쌓일까봐 20개까지 저장하고 앞을 삭제
				helmetHandles.erase(helmetHandles.begin());
			}

			a->UnequipArmorFromSlot((BIPED_OBJECT)i, true);
			break;
		}
	}

	isRunning = false;
}

void injectHeadEnchant(std::monostate)
{
	BSTArray<EquippedItem>* equipped = &p->currentProcess->middleHigh->equippedItems;
	if (!equipped)
		return;

	if (equipped->size() != 0 && (*equipped)[0].item.instanceData) {
		TESObjectWEAP* weapon = (TESObjectWEAP*)(*equipped)[0].item.object;

		if (!weapon || weapon->weaponData.ammo == nullptr)
			return;  // 무기가 없거나 투사 무기가 아니면 리턴

		// 장착중인 무기에서 인스턴트 데이터 가져오는 코드
		TESObjectWEAP::InstanceData* instData = (TESObjectWEAP::InstanceData*)((*equipped)[0].item.instanceData).get();

		if (!instData) {
			return;
		}

		// 장착한 무기의 타입을 저장함
		weaponType = WeaponType::NonSniper;

		BSTArray<EquippedItem>* equipped = &p->currentProcess->middleHigh->equippedItems;
		if (!equipped) {
			isRunning = false;
			return;
		}

		if (equipped->size() != 0 && (*equipped)[0].item.instanceData) {
			TESObjectWEAP* weap = (TESObjectWEAP*)(*equipped)[0].item.object;

			if (!weap) {
				isRunning = false;
				return;
			}

			if (HasKeywordVM(vm, 0, weap, WeaponTypeSniper)) {
				float vSpeed = weap->weaponData.rangedData->boltChargeSeconds;
				bool isBolt = vSpeed > 0.5;

				if (isBolt) {
					weaponType = WeaponType::BoltAction;  // 볼트액션일 경우
				} else {
					weaponType = WeaponType::Sniper;  // 볼트액션이 아니면 일반 저격총
				}
			}
		}

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

class ContainerMenuHandler : public BSTEventSink<MenuOpenCloseEvent>
{
public:
	virtual BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent& evn, BSTEventSource<MenuOpenCloseEvent>* src) override
	{
		if (evn.menuName == "VATSMenu") {
			if (evn.opening) {
				for (ObjectRefHandle& handle : helmetHandles) {
					auto perRef = handle.get();
					if (perRef) {
						TESObjectREFR* ref = perRef.get();
						if (ref) {
							ref->Disable();
						}
					}
				}
				helmetHandles.clear();
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}
};

void RegisterEvent()
{
	ContainerMenuHandler* menuHandle = new ContainerMenuHandler();
	ui->GetSingleton()->GetEventSource<MenuOpenCloseEvent>()->RegisterSink(menuHandle);
}

void createFileIfNotExist(const std::string& filePath)
{
	// 파일이 존재하지 않으면 생성
	if (!fs::exists(filePath)) {
		std::ofstream outFile(filePath);
	}
}

void OnF4SEMessage(F4SE::MessagingInterface::Message* msg)
{
	switch (msg->type) {
	case F4SE::MessagingInterface::kGameLoaded:
		{
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
			//Pop_NonCollision = (TESGlobal*)DH->LookupForm(0x81c, "HP_HelmetPopper.esp");
			Pop_Force_NonSniper = (TESGlobal*)DH->LookupForm(0x827, "HP_HelmetPopper.esp");

			// 실행 파일 경로를 구한 후 targetDirectory에 직접 할당
			char resultBuf[256];
			uint32_t tInt = GetModuleFileNameA(GetModuleHandle(NULL), resultBuf, sizeof(resultBuf));

			lootDir = std::string(resultBuf, tInt);
			lootDir = lootDir.substr(0, lootDir.find_last_of('\\')) + "\\Data\\F4SE\\Plugins\\_skipFilter\\";

			loadFilterSettingsFromFiles();  // txt 파일에 적힌 필터들을 배열에 저장함
			
			// 필터 파일을 확인하고 없으면 만듬
			std::string getName = "skipFilter.txt";

			// Get.txt와 Skip.txt 경로 생성
			std::string getPath = lootDir + getName;

			// 파일이 없으면 생성
			createFileIfNotExist(getPath);

			// 필터가구를 닫을때 쓸 변수들
			filterBox = (TESObjectREFR*)DH->LookupForm(0x823, "HP_HelmetPopper.esp");

			RegisterEvent();

			break;
		}
	case F4SE::MessagingInterface::kPostLoadGame:
		{
			injectHeadEnchant(std::monostate{});

			break;
		}
	}
}

bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
{
	vm = a_vm;
	a_vm->BindNativeMethod("HP_HelmetPopper"sv, "HitHead"sv, HitHead);
	a_vm->BindNativeMethod("HP_HelmetPopper"sv, "injectHeadEnchant"sv, injectHeadEnchant);
	a_vm->BindNativeMethod("HP_HelmetPopper"sv, "setSkipFilter"sv, setSkipFilter);
	a_vm->BindNativeMethod("HP_HelmetPopper"sv, "FillContainerfromFile", FillContainerfromFile);

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
