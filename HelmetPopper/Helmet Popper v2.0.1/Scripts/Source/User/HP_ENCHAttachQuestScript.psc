ScriptName HP_ENCHAttachQuestScript Extends ReferenceAlias

Keyword Property WeaponTypeSniper Auto

Actor Property PlayerRef Auto

Event OnItemEquipped(Form akBaseObject, ObjectReference akReference)
	if akBaseObject as Weapon
		HP_HelmetPopper.injectHeadEnchant()
	Endif
EndEvent