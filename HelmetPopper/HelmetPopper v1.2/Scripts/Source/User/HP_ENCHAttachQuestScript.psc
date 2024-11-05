ScriptName HP_ENCHAttachQuestScript Extends ReferenceAlias

Keyword Property WeaponTypeSniper Auto

Actor Property PlayerRef Auto

Event OnItemEquipped(Form akBaseObject, ObjectReference akReference)
	if akBaseObject as Weapon
		if akBaseObject.HasKeyword(WeaponTypeSniper)
			HP_HelmetPopper.injectHeadEnchant()
		Endif
	Endif
EndEvent