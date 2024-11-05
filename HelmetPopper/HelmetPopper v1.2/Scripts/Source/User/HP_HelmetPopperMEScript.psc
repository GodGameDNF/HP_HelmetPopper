ScriptName HP_HelmetPopperMEScript Extends ActiveMagicEffect

Event OnEffectStart(Actor akTarget, Actor akCaster)
	HP_HelmetPopper.HitHead(akTarget)
EndEvent