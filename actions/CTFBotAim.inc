enum LookAtPriorityType
{
	BORING       = 0,
	INTERESTING  = 1,
	IMPORTANT    = 2,
	CRITICAL     = 3,
	OVERRIDE_ALL = 4,
};

ConVar g_hDebug;
ConVar g_hAimRate; 
ConVar g_hHeadSteadyRate;
ConVar g_hSaccadeSpeed;
ConVar g_hHeadResettleAngle;
ConVar g_hHeadResettleTime;
ConVar g_hHeadAimSettleDuration;

float m_angLastEyeAngles[MAXPLAYERS + 1][3];
float m_vecAimTarget[MAXPLAYERS + 1][3];
float m_vecTargetVelocity[MAXPLAYERS + 1][3];
float m_vecLastEyeVectors[MAXPLAYERS + 1][3];

float m_ctAimTracking[MAXPLAYERS + 1];
float m_ctAimDuration[MAXPLAYERS + 1];
float m_ctResettle[MAXPLAYERS + 1];

//Buttons
float m_ctFire[MAXPLAYERS + 1];
float m_ctAltFire[MAXPLAYERS + 1];
float m_ctReload[MAXPLAYERS + 1];

float m_itAimStart[MAXPLAYERS + 1];
float m_itHeadSteady[MAXPLAYERS + 1];

LookAtPriorityType m_iAimPriority[MAXPLAYERS + 1];
int m_hAimTarget[MAXPLAYERS + 1];

bool m_bHeadOnTarget[MAXPLAYERS + 1];
bool m_bSightedIn[MAXPLAYERS + 1];

//All this is updated at:
//nb_update_frequency
public void InitTFBotAim()
{
	g_hDebug                 = CreateConVar("sm_idlebot_debug",                      "0.0",    "Debug aiming", _, true, 0.0, true, 1.0);
	g_hAimRate               = CreateConVar("sm_idlebot_head_aim_tracking_interval", "0.2",    "Aim Recalculate Interval");
	g_hHeadSteadyRate        = CreateConVar("sm_idlebot_head_aim_steady_max_rate",   "100.0",  "Head aim steady max rate");
	g_hSaccadeSpeed          = CreateConVar("sm_idlebot_saccade_speed",              "1000.0", "Max head angular velocity");
	g_hHeadResettleAngle     = CreateConVar("sm_idlebot_head_resettle_angle",        "100.0",  "After rotating through this angle, the bot pauses to 'recenter' its virtual mouse on its virtual mousepad");
	g_hHeadResettleTime      = CreateConVar("sm_idlebot_head_resettle_time",         "0.3",    "How long the bot pauses to 'recenter' its virtual mouse on its virtual mousepad");
	g_hHeadAimSettleDuration = CreateConVar("sm_idlebot_head_aim_settle_duration",   "0.3",    "");
}

methodmap BotAim
{
	public BotAim(int client)
	{
		return view_as<BotAim>(client);
	}
	public void Reset()
	{
		m_angLastEyeAngles[view_as<int>(this)] = NULL_VECTOR;
		m_vecAimTarget[view_as<int>(this)] = NULL_VECTOR;
		m_vecTargetVelocity[view_as<int>(this)] = NULL_VECTOR;
		m_vecLastEyeVectors[view_as<int>(this)] = NULL_VECTOR;
		
		m_ctAimTracking[view_as<int>(this)] = -1.0;
		m_ctAimDuration[view_as<int>(this)] = -1.0;
		m_ctResettle[view_as<int>(this)] = -1.0;
		
		m_itAimStart[view_as<int>(this)] = -1.0;
		m_itHeadSteady[view_as<int>(this)] = -1.0;
		
		m_hAimTarget[view_as<int>(this)] = -1;
		m_iAimPriority[view_as<int>(this)] = BORING;
		
		m_bHeadOnTarget[view_as<int>(this)] = false;
		m_bSightedIn[view_as<int>(this)] = false;
		
		m_ctFire[view_as<int>(this)] = -1.0;
		m_ctAltFire[view_as<int>(this)] = -1.0;
		m_ctReload[view_as<int>(this)] = -1.0;
	}	
	property int index
	{
		public get() 
		{ 
			return view_as<int>(this);
		}
	}	
	public bool IsHeadAimingOnTarget()
	{
		return m_bHeadOnTarget[view_as<int>(this)];
	}	
	public bool IsHeadSteady()
	{
		return view_as<bool>(m_itHeadSteady[view_as<int>(this)] != -1);
	}	
	public float GetHeadSteadyDuration()
	{
		if (m_itHeadSteady[view_as<int>(this)] == -1) {
			return 0.0;
		}
		
		return GetGameTime() - m_itHeadSteady[view_as<int>(this)];
	}	
	public float GetMaxHeadAngularVelocity()
	{
		return g_hSaccadeSpeed.FloatValue;
	}	
	
	public void ReleaseFireButton()
	{
		m_ctFire[view_as<int>(this)] = 0.0;
	}
	public void ReleaseAltFireButton()
	{
		m_ctAltFire[view_as<int>(this)] = 0.0;
	}
	
	public void PressFireButton(float duration = 0.1)
	{
		m_ctFire[view_as<int>(this)] = GetGameTime() + duration;
	}	
	public void PressAltFireButton(float duration = 0.1)
	{
		m_ctAltFire[view_as<int>(this)] = GetGameTime() + duration;
	}
	public void PressReloadButton(float duration = 0.1)
	{
		m_ctReload[view_as<int>(this)] = GetGameTime() + duration;
	}
	
	public void Upkeep()
	{
		float frametime = GetGameFrameTime();
		if(frametime < 0.00001)
			return;
		
		//Don't aim while taunting.
		if(TF2_IsPlayerInCondition(this.index, TFCond_Taunting))
			return;
		
		float eye_ang[3], punch_angle[3];
		GetClientEyeAngles(this.index, eye_ang);
		GetEntPropVector(this.index, Prop_Send, "m_vecPunchAngle", punch_angle);
		AddVectors(eye_ang, punch_angle, eye_ang);
	
		if (FloatAbs(float(RoundToFloor(AngleDiff(eye_ang[0], m_angLastEyeAngles[view_as<int>(this)][0])))) > (frametime * g_hHeadSteadyRate.FloatValue)
		 || FloatAbs(float(RoundToFloor(AngleDiff(eye_ang[1], m_angLastEyeAngles[view_as<int>(this)][1])))) > (frametime * g_hHeadSteadyRate.FloatValue))
		{
			m_itHeadSteady[view_as<int>(this)] = -1.0;	//this->m_itHeadSteady.Invalidate();
		} 
		else 
		{
			if (m_itHeadSteady[view_as<int>(this)] == -1) 
			{
				m_itHeadSteady[view_as<int>(this)] = GetGameTime();
			}
		}
		
		m_angLastEyeAngles[view_as<int>(this)] = eye_ang;
		
		if (m_bSightedIn[view_as<int>(this)] && m_ctAimDuration[view_as<int>(this)] <= GetGameTime()) {
			return;
		}
		
		float eye_vec[3];
		GetAngleVectors(eye_ang, eye_vec, NULL_VECTOR, NULL_VECTOR);
		
		if(ArcCosine(GetVectorDotProduct(m_vecLastEyeVectors[view_as<int>(this)], eye_vec)) * (180.0 / FLOAT_PI) > g_hHeadResettleAngle.FloatValue)
		{
			m_ctResettle[view_as<int>(this)] = GetGameTime() + g_hHeadResettleTime.FloatValue * GetRandomFloat(0.9, 1.1);
			m_vecLastEyeVectors[view_as<int>(this)] = eye_vec;
		}
		else if (m_ctResettle[view_as<int>(this)] == -1 || m_ctResettle[view_as<int>(this)] <= GetGameTime())
		{
			m_ctResettle[view_as<int>(this)] = -1.0;
			
			int target_ent = m_hAimTarget[view_as<int>(this)];
						
			if (target_ent > 0 && target_ent <= MaxClients && IsClientInGame(target_ent)) 
			{
				float target_velocity[3]; target_velocity = GetAbsVelocity(target_ent);
			
				float target_point[3]; 
				
				//Grenade launcher Arc
				if(IsWeapon(this.index, TF_WEAPON_GRENADELAUNCHER) 
				|| IsWeapon(this.index, TF_WEAPON_PIPEBOMBLAUNCHER))
				{
					target_point = WorldSpaceCenter(target_ent); //GetAbsOrigin(target_ent);
				
					float vecTarget[3], vecActor[3];
					vecTarget = GetAbsOrigin(target_ent);
					vecActor = GetAbsOrigin(this.index);
					
					float flDistance = GetVectorDistance(vecTarget, vecActor);
					if (flDistance > 150.0)
					{
						flDistance = flDistance / GetProjectileSpeed(this.index);
						
						float AbsVelocity[3]; AbsVelocity = GetAbsVelocity(target_ent);
						
						target_point[0] = vecTarget[0] + AbsVelocity[0] * flDistance;
						target_point[1] = vecTarget[1] + AbsVelocity[1] * flDistance;
						target_point[2] = vecTarget[2] + AbsVelocity[2] * flDistance;
					}
					else
					{
						target_point = WorldSpaceCenter(target_ent);
					}
					
					float vecToTarget[3]; 
					SubtractVectors(target_point, vecActor, vecToTarget);
					
					float a5 = VMX_VectorNormalize(vecToTarget);
					
					float flBallisticElevation = /*FindConVar("tf_bot_ballistic_elevation_rate").FloatValue*/0.0125 * a5;
					if ( flBallisticElevation > 45.0 )
						flBallisticElevation = 45.0;
						
					float flElevation = flBallisticElevation * (FLOAT_PI / 180.0);
					
					float sineValue   = Sine(flElevation);
					float cosineValue = Cosine(flElevation);
					
					if (cosineValue != 0.0)
					{
						target_point[2] += (sineValue * a5) / cosineValue;
					}
				}
				//Huntsman arc
				else if(IsWeapon(this.index, TF_WEAPON_COMPOUND_BOW))
				{
					float vecTarget[3]; vecTarget = GetAbsOrigin(target_ent);
					float vecActor[3];  vecActor  = GetAbsOrigin(this.index);
					
					float flDistance = GetVectorDistance(vecTarget, vecActor);
					if (flDistance <= 150.0)
					{
						//Just aim at head if they're this close.
						target_point = GetEyePosition(target_ent);
					}
					else
					{
						float targeteyepos[3]; targeteyepos = GetEyePosition(target_ent);
						
						float targetvelocity[3]; targetvelocity = GetAbsVelocity(target_ent);
						
						float flProjectileSpeed = GetProjectileSpeed(this.index);
	
						float flDistanceProjectileSpeed = flDistance / flProjectileSpeed;
						
						target_point[0] = (targetvelocity[0] * flDistanceProjectileSpeed) + targeteyepos[0];
						target_point[1] = (targetvelocity[1] * flDistanceProjectileSpeed) + targeteyepos[1];
						
						float flElevation = (flDistance * FindConVar("tf_bot_arrow_elevation_rate").FloatValue);
						if(flElevation > 45.0)
							flElevation = 45.0;
						
						float flGravity = GetProjectileGravity(this.index);	
						flElevation *= (flGravity + (FLOAT_PI / 180.0));
						
						float sineValue   = Sine(flElevation);
						float cosineValue = Cosine(flElevation);
						
						if ( cosineValue == 0.0 )
						{
							target_point[2] = ((targetvelocity[2] * flDistanceProjectileSpeed) + targeteyepos[2]);
						}
						else
						{
							float math = ((targetvelocity[2] * flDistanceProjectileSpeed) + targeteyepos[2]);
							target_point[2] = ((flDistance * sineValue) / cosineValue) + math;
						}
					}
				}
				//Aim at head with sniper-rifles.
				else if(IsSniperRifle(this.index))
				{
					int iBone = LookupBone(target_ent, "bip_head");
					if(iBone != -1)
					{
						float vNothing[3];
						GetBonePosition(target_ent, iBone, target_point, vNothing);
						target_point[2] += 3.0;
					}
					else
					{
						target_point = GetEyePosition(target_ent);
					}
				}
				//Aim ahead with rocket-launchers.
				else if(IsWeapon(this.index, TF_WEAPON_ROCKETLAUNCHER)
				     || IsWeapon(this.index, TF_WEAPON_DIRECTHIT)
				     || IsWeapon(this.index, TF_WEAPON_PARTICLE_CANNON))
				{
					float vecTarget[3], vecActor[3];
					vecTarget = GetAbsOrigin(target_ent);
					vecActor = GetAbsOrigin(this.index);
					
					float flDistance = GetVectorDistance(vecTarget, vecActor);
					if (flDistance > 150.0)
					{
						flDistance = flDistance * 0.00090909092;
						
						float AbsVelocity[3]; AbsVelocity = GetAbsVelocity(target_ent);
						
						target_point[0] = vecTarget[0] + AbsVelocity[0] * flDistance;
						target_point[1] = vecTarget[1] + AbsVelocity[1] * flDistance;
						target_point[2] = vecTarget[2] + AbsVelocity[2] * flDistance;
						
						//If we can't shoot at the feet shoot at the center.
						if(!IsLineOfFireClear(GetEyePosition(this.index), target_point))
						{
							vecTarget = WorldSpaceCenter(target_ent);
						
							target_point[0] = vecTarget[0] + AbsVelocity[0] * flDistance;
							target_point[1] = vecTarget[1] + AbsVelocity[1] * flDistance;
							target_point[2] = vecTarget[2] + AbsVelocity[2] * flDistance;
						}
					}
					else
					{
						target_point = WorldSpaceCenter(target_ent);
					}
				}
				//Normally aim at center of target.
				else
				{
					target_point = WorldSpaceCenter(target_ent);
				}
				
				if (m_ctAimTracking[view_as<int>(this)] <= GetGameTime()) 
				{
					float delta[3]
					SubtractVectors(target_point, m_vecAimTarget[view_as<int>(this)], delta);
					
					float flLeadTime = 0.0;
					delta[0] += (flLeadTime * target_velocity[0]);
					delta[1] += (flLeadTime * target_velocity[1]);
					delta[2] += (flLeadTime * target_velocity[2]);
					
					float track_interval = Max(frametime, g_hAimRate.FloatValue);
					float scale = GetVectorLength(delta) / track_interval;
					NormalizeVector(delta, delta);
					
					m_vecTargetVelocity[view_as<int>(this)][0] = (scale * delta[0]) + target_velocity[0];
					m_vecTargetVelocity[view_as<int>(this)][1] = (scale * delta[1]) + target_velocity[1];
					m_vecTargetVelocity[view_as<int>(this)][2] = (scale * delta[2]) + target_velocity[2];
					
					m_ctAimTracking[view_as<int>(this)] = GetGameTime() + (track_interval * GetRandomFloat(0.8, 1.2));
				}
				
				m_vecAimTarget[view_as<int>(this)][0] += frametime * m_vecTargetVelocity[view_as<int>(this)][0];
				m_vecAimTarget[view_as<int>(this)][1] += frametime * m_vecTargetVelocity[view_as<int>(this)][1];
				m_vecAimTarget[view_as<int>(this)][2] += frametime * m_vecTargetVelocity[view_as<int>(this)][2];
			}
		}
	
		float eye_to_target[3], myEyePosition[3];
		GetClientEyePosition(this.index, myEyePosition);
		SubtractVectors(m_vecAimTarget[view_as<int>(this)], myEyePosition, eye_to_target);
		
		NormalizeVector(eye_to_target, eye_to_target);
		
		float ang_to_target[3];
		GetVectorAngles(eye_to_target, ang_to_target);
		
		float cos_error = GetVectorDotProduct(eye_to_target, eye_vec);
		
		/* must be within ~11.5 degrees to be considered on target */
		if (cos_error <= 0.98)
		{
			m_bHeadOnTarget[view_as<int>(this)] = false;
		}
		else 
		{
			m_bHeadOnTarget[view_as<int>(this)] = true;
			
			if (!m_bSightedIn[view_as<int>(this)]) 
			{
				m_bSightedIn[view_as<int>(this)] = true;
				
				if (g_hDebug.BoolValue) 
				{
					PrintToServer("%3.2f: %N Look At SIGHTED IN\n",
						GetGameTime(), this);
				}
			}
		}
		
		float max_angvel = this.GetMaxHeadAngularVelocity();
		
		/* adjust angular velocity limit based on aim error amount */
		if (cos_error > 0.7)
		{
			max_angvel *= Sine((3.14 / 2.0) * (1.0 + ((-49.0 / 15.0) * (cos_error - 0.7))));
		}
	
		if(m_itAimStart[view_as<int>(this)] != -1 && (GetGameTime() - m_itAimStart[view_as<int>(this)] < 0.25))
		{
			max_angvel *= 4.0 * (GetGameTime() - m_itAimStart[view_as<int>(this)]);
		}
		
		float new_eye_angle[3];
		new_eye_angle[0] = ApproachAngle(ang_to_target[0], eye_ang[0], (max_angvel * frametime) * 0.5);
		new_eye_angle[1] = ApproachAngle(ang_to_target[1], eye_ang[1], (max_angvel * frametime));
		new_eye_angle[2] = 0.0;
		
		SubtractVectors(new_eye_angle, punch_angle, new_eye_angle);
		new_eye_angle[0] = AngleNormalize(new_eye_angle[0]);
		new_eye_angle[1] = AngleNormalize(new_eye_angle[1]);
		new_eye_angle[2] = 0.0;
		
		//PrintToServer("x %f y %f z %f", new_eye_angle[0], new_eye_angle[1], new_eye_angle[2]);
		SnapEyeAngles(this.index, new_eye_angle);
	//	TeleportEntity(this.index, NULL_VECTOR, new_eye_angle, NULL_VECTOR);
	}
	public void AimHeadTowards(const float vec[3], LookAtPriorityType priority, float duration = 0.0, const char[] reason)
	{
		if (duration <= 0.0) {
			duration = 0.1;
		}
		
		if (priority == m_iAimPriority[view_as<int>(this)] && (!this.IsHeadSteady() || this.GetHeadSteadyDuration() < g_hHeadAimSettleDuration.FloatValue)) 
		{
			if (g_hDebug.BoolValue) 
			{
				PrintToServer("%3.2f: %N Look At '%s' rejected - previous aim not %s\n",
					GetGameTime(), this, reason, (this.IsHeadSteady() ? "settled long enough" : "head-steady"));
			}
		}
		
		if (priority > m_iAimPriority[view_as<int>(this)] || m_ctAimDuration[view_as<int>(this)] <= GetGameTime()) 
		{
			m_ctAimDuration[view_as<int>(this)] = GetGameTime() + duration;
			m_iAimPriority[view_as<int>(this)] = priority;
			
			/* only update our aim if the target vector changed significantly */
			if (GetVectorDistance(vec, m_vecAimTarget[view_as<int>(this)]) >= 1.0)
			{
				m_hAimTarget[view_as<int>(this)] = -1;
				m_vecAimTarget[view_as<int>(this)] = vec;
				m_itAimStart[view_as<int>(this)] = GetGameTime();
				m_bHeadOnTarget[view_as<int>(this)] = false;
				
				if (g_hDebug.BoolValue) 
				{
					char pri_str[16];
					switch (priority) 
					{
						case BORING:      pri_str = "Boring";
						case INTERESTING: pri_str = "Interesting";
						case IMPORTANT:   pri_str = "Important";
						case CRITICAL:    pri_str = "Critical";
					}
					
					PrintToServer("%3.2f: %N Look At ( %f, %f, %f ) for %3.2f s, Pri = %s, Reason = %s\n", GetGameTime(), this, vec[0], vec[1], vec[2], duration, pri_str, reason);
				}
			}
		} 
		else 
		{
			if (g_hDebug.BoolValue) 
			{
				PrintToServer("%3.2f: %N Look At '%s' rejected - higher priority aim in progress\n", 
					GetGameTime(), this, reason);
			}
			
			return;
		}
	}
	public void AimHeadTowardsEntity(int ent, LookAtPriorityType priority, float duration = 0.0, const char[] reason)
	{	
		if (duration <= 0.0) {
			duration = 0.1;
		}
		
		if (priority == m_iAimPriority[view_as<int>(this)] && (!this.IsHeadSteady() || this.GetHeadSteadyDuration() < g_hHeadAimSettleDuration.FloatValue)) 
		{
			if (g_hDebug.BoolValue) 
			{
				PrintToServer("%3.2f: %N Look At '%s' rejected - previous aim not %s\n",
					GetGameTime(), this, reason, (this.IsHeadSteady() ? "settled long enough" : "head-steady"));
			}
		}
		
		if (priority > m_iAimPriority[view_as<int>(this)] || m_ctAimDuration[view_as<int>(this)] <= GetGameTime()) 
		{
			m_ctAimDuration[view_as<int>(this)] = GetGameTime() + duration;
			m_iAimPriority[view_as<int>(this)] = priority;
			
			/* only update our aim if the target entity changed */
			int prev_target = m_hAimTarget[view_as<int>(this)];
			if (prev_target == -1 || ent != prev_target) 
			{
				m_hAimTarget[view_as<int>(this)] = ent;
				m_itAimStart[view_as<int>(this)] = GetGameTime();
				m_bHeadOnTarget[view_as<int>(this)] = false;

				m_itHeadSteady[view_as<int>(this)] = -1.0;
				m_bSightedIn[view_as<int>(this)] = false;

				if (g_hDebug.BoolValue) 
				{
					char pri_str[16];
					switch (priority) 
					{
						case BORING:      pri_str = "Boring";
						case INTERESTING: pri_str = "Interesting";
						case IMPORTANT:   pri_str = "Important";
						case CRITICAL:    pri_str = "Critical";
					}
					
					char strClass[64];
					GetEntityClassname(ent, strClass, sizeof(strClass));
					
					PrintToServer("%3.2f: %N Look At subject %s for %3.2f s, Pri = %s, Reason = %s\n",
						GetGameTime(), this, strClass, duration, pri_str, reason);
				}
			}
		}
		else 
		{			
			if (g_hDebug.BoolValue) 
			{
				PrintToServer("%3.2f: %N Look At '%s' rejected - higher priority aim in progress\n",
					GetGameTime(), this, reason);
			}
		}
	}
	public void FireWeaponAtEnemy()
	{
		if(!IsPlayerAlive(this.index))
			return;
			
		if(GetActiveWeapon(this.index) == INVALID_ENT_REFERENCE)
			return;
		
		if(!FindConVar("tf_bot_fire_weapon_allowed").BoolValue)
			return;
		
		if(TF2_IsPlayerInCondition(this.index, TFCond_Taunting))
			return;
			
		if(!HasAmmo(GetActiveWeapon(this.index)))
			return;
			
		if(TF2_GetPlayerClass(this.index) == TFClass_Medic && IsWeapon(this.index, TF_WEAPON_MEDIGUN))
			return;
		
		if(TF2_GetPlayerClass(this.index) == TFClass_Heavy && !IsAmmoLow(this.index))
		{
			if (GetTimeSinceWeaponFired(this.index) < 3.0)
			{
				this.PressAltFireButton();
			}
		}
		
		int threat = m_hAimTarget[view_as<int>(this)]; //this->GetVisionInterface()->GetPrimaryKnownThreat(false);
	//	if (threat = nullptr || threat->GetEntity() == nullptr || !threat->IsVisibleRecently()) 
		if (threat <= 0 || threat > MaxClients || !IsClientInGame(threat) || !IsPlayerAlive(threat) || IsInvulnerable(threat)) 
		{
			m_hAimTarget[view_as<int>(this)] = -1;
			
			//Unscope because target isnt valid.
			//if (IsSniperRifle(this.index) && TF2_IsPlayerInCondition(this.index, TFCond_Zoomed)/* && !GetEntProp(this.index, Prop_Send, "m_bRageDraining")*/)
			//{
				//this.PressAltFireButton(0.1);
			//}
			
			return;
		}
		
		if (!IsLineOfFireClear(GetEyePosition(this.index), GetEyePosition(threat))   &&
			!IsLineOfFireClear(GetEyePosition(this.index), WorldSpaceCenter(threat)) &&
			!IsLineOfFireClear(GetEyePosition(this.index), GetAbsOrigin(threat)))
		{
			return;
		}
		
		if (GameRules_GetProp("m_bInSetup"))
			return;
		
		if (IsMeleeWeapon(this.index)) 
		{
			if (GetVectorDistance(GetAbsOrigin(this.index), GetAbsOrigin(threat)) < GetDesiredAttackRange(this.index)) 
			{
				this.PressFireButton();
			}
			
			return;
		}
		
		if (TF2_IsMvM()) 
		{
			if (TF2_GetPlayerClass(this.index) != TFClass_Sniper && IsHitScanWeapon(this.index) 
			&& GetVectorDistance(GetAbsOrigin(this.index), GetAbsOrigin(threat)) > FindConVar("tf_bot_hitscan_range_limit").FloatValue)
			{
				return;
			}
		}
		
		if (IsWeapon(this.index, TF_WEAPON_FLAMETHROWER))
		{
		/*	CTFFlameThrower *flamethrower = static_cast<CTFFlameThrower *>(weapon);
			if (flamethrower->CanAirBlast() && actor->ShouldFireCompressionBlast()) 
			{
				actor->PressAltFireButton();
				return;
			}*/
			
	/*		if (threat->GetTimeSinceLastSeen() < 1.0) 
			{
				float threat_to_actor[3];
				SubtractVectors(GetAbsOrigin(this.index), GetAbsOrigin(threat), threat_to_actor);
				
				if(GetVectorLength(threat_to_actor) < GetMaxAttackRange(this.index)) 
				{
					this.PressFireButton(FindConVar("tf_bot_fire_weapon_min_time").FloatValue);
				}
			}*/
			
			float threat_to_actor[3];
			SubtractVectors(GetAbsOrigin(this.index), GetAbsOrigin(threat), threat_to_actor);
			
			if(GetVectorLength(threat_to_actor) < GetMaxAttackRange(this.index)) 
			{
				this.PressFireButton(FindConVar("tf_bot_fire_weapon_min_time").FloatValue);
			}
		
			return;
		}
		
		float actor_to_threat[3]; SubtractVectors(GetAbsOrigin(threat), GetAbsOrigin(this.index), actor_to_threat);
		float dist_to_threat = GetVectorLength(actor_to_threat);
		
		if(!this.IsHeadAimingOnTarget())
			return;
		
		if (dist_to_threat >= GetMaxAttackRange(this.index)) 
			return;
		
		if (IsWeapon(this.index, TF_WEAPON_COMPOUND_BOW))
		{
			//PrintCenterText(this.index, "%f", GetCurrentCharge(GetActiveWeapon(this.index)));
			
			if (GetCurrentCharge(GetActiveWeapon(this.index)) >= 0.5 && IsLineOfFireClear(GetEyePosition(this.index), WorldSpaceCenter(threat)))
			{
				return;
			}
			
			this.PressFireButton();
			return;
		}
		
		if (IsSniperRifle(this.index))
		{
			// TODO: bunch of stuff related to IntervalTimer @ 0x5c
			// ...
			
		/*	if (!TF2_IsPlayerInCondition(this.index, TFCond_Zoomed))
			{
				//Scope in
				this.PressAltFireButton(0.1);
			}*/
			if(HasEntProp(GetActiveWeapon(this.index), Prop_Send, "m_flChargedDamage")
				&& GetEntPropFloat(GetActiveWeapon(this.index), Prop_Send, "m_flChargedDamage") >= 40.0 
				&& this.IsHeadSteady())
			{
				//Shoot
				this.PressFireButton();
			}
			
			return;
		}
		
		if(!IsCombatWeapon(this.index))
			return;
		
		if(IsContinuousFireWeapon(this.index))
		{
			this.PressFireButton(FindConVar("tf_bot_fire_weapon_min_time").FloatValue / 2);
			return;
		}
		
		if (IsExplosiveProjectileWeapon(this.index)) 
		{
			float aim_vec[3];
			EyeVectors(this.index, aim_vec, NULL_VECTOR, NULL_VECTOR);
			ScaleVector(aim_vec, (1.1 * dist_to_threat));
			
			float vecEyeDirection[3];
			AddVectors(GetEyePosition(this.index), aim_vec, vecEyeDirection);
			
			Handle trace = TR_TraceRayFilterEx(GetEyePosition(this.index), vecEyeDirection, MASK_SHOT, RayType_EndPoint, TraceFilterSelf, this.index);
			
			int iEntityHit = TR_GetEntityIndex(trace);
			if ((TR_GetFraction(trace) * (1.1 * dist_to_threat)) < 146.0 &&	iEntityHit == -1)
			{
				delete trace;
				return;
			}
			
			delete trace;
			
			if(this.IsHeadSteady())
			{
				this.PressFireButton();	
			}
			
			return;
		}
		
		this.PressFireButton();
	}
}