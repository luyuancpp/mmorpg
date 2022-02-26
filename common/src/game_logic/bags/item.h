#ifndef ITEM_H_
#define ITEM_H_

#include <cstddef>
#include <functional>
#include <vector>
#include <list>
#include "StlDefineType.h"
#include "Game/BaseDef.h"
#include "PublicStruct.pb.h"
#include "InterNode.pb.h"
#include "Game/Snowflake.h"
#include "LuaScript/IScriptHost.h"



class SyncDataBaseAttrV1;
class Obj_Human;


#define NULL_ITEM_CONFIG -1

struct AttrContainer
{
    int attr_id;
    int attr_weight;
    int attr_min;
    int attr_max;

    AttrContainer(const std::vector<int>& config, bool refine=false)
    {
        if (refine)
        {
            if (config.size() < 6)
            {
                return;
            }
        }
        else
        {
            if (config.size() < 4)
            {
                return;
            }
        }
        attr_id = config[0];
        attr_weight = config[1];

        if (refine)
        {
            attr_min = config[4];
            attr_max = config[5];
        }
        else
        {
            attr_min = config[2];
            attr_max = config[3];
        }
    }
};


struct AttrQuality
{
    int attr_weight;
    AttrQualityEnum::Type quality_type;

    AttrQuality(int _attr_weight, AttrQualityEnum::Type _quality_type)
    {
        attr_weight = _attr_weight;
        quality_type = _quality_type;
    }
};

namespace HumanBag
{
class CreateItemParam;
class ItemParam;
class Item

	: public IScriptHost

{
public:

	enum ItemEnum
	{
		E_CURRENCY = 1,
		E_EQUIPMENT = 2,
		E_MATERIAL = 3,
		E_GEM = 4,
		E_CONSUMABLE = 5,
		E_POTION = 6,
		E_GIFT = 7,
		E_PET = 8,			//宠物
        E_OTHER_UESE_ITEM = 9,
		E_PET_SKILL_BOOK = 10,//宠物技能书
		E_SOUL_STONE = 11,//魂石
		E_GUILD_COTRIBUTE = 12,//公会贡献度道具
		E_ITEMOPTION = 13,//自选礼包
		E_MOUNT = 14,
        E_EMBLEM = 15,
        E_DRESSUP = 18,
        E_MOUNT_EQUIP = 19,//坐骑装备
		E_TYPE_MAX = 1000,
	};
    enum eItemQulityEnum
    {
        E_Quailty0,
        E_Quailty1,
        E_Quailty2,
        E_Quailty3,
        E_Quailty4,
        E_Quailty5,
        E_Quailty_MAX,
    };

	typedef std::function<int32_t (std::size_t)> check_capacity_cb_type;
    typedef std::function<void(GUID_t)> item_guid_callback_type;
    typedef std::shared_ptr<Item> item_type;
    typedef std::function<void(const ItemParam&)> remove_item_cbtype;
    using get_equip_by_slottype = std::function<item_type(EquipTypeEnum::Type)>;
    // construct by proto and count, will gen special behavior
	Item(const ItemProtoElement * pItemProtoElement, int32_t nCount);
    // construct by pb, keep pb original
    Item(const ItemData & pb);
    Item();
    virtual ~Item();

	virtual void OnLoad(const ItemData & pb);
	virtual void OnSave(ItemData & pb);

	void ToClientPb(ItemData & pb);
	void ToJson(Json::Value& item);

	virtual INT GetConfId();

	virtual int32_t OnUse(ItemParam & nUseCondition);
    virtual int32_t OnRemove(ItemParam & nUseCondition);
	virtual void OnNewDayCome() {};

	int32_t OnSale(ItemParam & nUseCondition);
    void OnChange(ItemParam & nUseCondition);

	virtual int32_t CheckLockUse();
	virtual int32_t CheckUse(ItemParam & nUseCondition);
	virtual int32_t CheckSale(ItemParam & nUseCondition);
	virtual int32_t CheckUseTarget(ItemParam & nUseCondition);

	int32_t EnoughLevel(ItemParam & nUseCondition);
	int32_t CheckEnoughOverlaySize(ItemParam & nUseCondition);
	int32_t EnoughBagCapacity(ItemParam & nUseCondition);

	virtual std::string GetItemName() const;


	//Pet Start
	virtual int32_t GetRank() const { return INVALID_ID; } //星级
	virtual int32_t GetLevel() const override { return INVALID_ID; }
	virtual void SetWild(bool bWild) {}
	virtual bool IsWild() { return false; }
	virtual bool IsRefined()const { return false; };
	//Pet End

	//检查是否满足职业需求
	virtual int32_t CheckOccupation(int32_t nOccupation) const;

    //可合并
	int32_t CanMerge();
	//可交易
	int32_t CanTrade();
    //可分解
    int32_t CanBreak(IScriptHost * pIScriptHost, ItemParam & p);

    int32_t CanEquip(int nPlayerLevel, int nOccupation) const;
    int32_t CanEquipEmblem(int nPlayerLevel) const;

    inline const ItemProtoElement * GetItemProtoElement()const
	{
		return m_pItemProtoElement;
	}

    void SetItemProtoElement(const ItemProtoElement *p)
	{
		m_pItemProtoElement = p;
	}
	
    int32_t GetItemType()const;

    bool IsGad() const;

    virtual int32_t GetQuality()const;


    int32_t GetOriginalQuality() const;

    virtual item_type Clone();

	bool operator < (const  Item &)const;
	
	int32_t AddOverlaySize(int32_t nOverlaySize, CreateItemParam & param);
    int32_t AddRedPoint(CreateItemParam & param);

    int32_t DelOverlaySize(int32_t nOverlaySize);

	inline  int32_t GetOverlaySize()const
	{
		return m_oData.overlaycount();
	}

    inline bool IsFull()const;


    inline bool GetLock()const
	{
		return m_oData.islock();
	}

    inline void SetLock(bool bLock)
	{
		m_oData.set_islock(bLock);
	}

	bool GetBind()const
	{
		return m_oData.isbind();
	}

	void SetBind(bool bBind)
	{
		m_oData.set_isbind(bBind);
	}
	int32_t GetBindRule() const
	{
		return m_oData.bindrule();
	}
	void SetBindRule(int32_t nRule)
	{
		m_oData.set_bindrule(nRule);
	}
	void SetSaleCd(int32_t cd)
	{
		m_oData.set_salecd(cd);
	}
	int32_t GetSaleCd() const
	{
		return m_oData.salecd();
	}

	int32_t GetItemId() const
	{
		return m_oData.configid();
	}
	virtual int32_t GetBattleScore()
	{
		if (m_oData.has_soulstone())
		{
			return m_oData.soulstone().battlescore();
		}
		return 0;
	}

    virtual bool CanRefining() { return false; }

    //config id
    int32_t GetId() const { return m_oData.configid(); }

	void SetCheckCapacityCallback(const check_capacity_cb_type & cb)
	{
		m_bCheckCapacityCallback = cb;
	}

    void SetItemChange(const item_guid_callback_type & cb)
    {
        item_change_callback_ = cb;
    }

    void SetAddItemGuid(const item_guid_callback_type & cb)
    {
        add_guid_item_callback_ = cb;
    }

    void SetGetEquipBySlotType(const get_equip_by_slottype& oCB)
    {
        m_oGetEquipBySlotType = oCB;
    }

    

	//GUID_t GetGuid()const
	//{
	//	return m_oData.guid();
	//}

	bool IsEmptyItem()const
	{
		return m_oData.configid() <= 0 || m_oData.overlaycount() <= 0 || GetItemProtoElement() == NULL;
	}

    void buildDBData(ItemData* itemData) const
    {
		if (nullptr != itemData)
		{
			itemData->CopyFrom(m_oData);
		}
        
    }

    //得到这个物品在背包里面占用的格子
    int32_t BeforeAddToBagPlaceholdersSize()const;
    int32_t GetFullPlaceholdersSize()const;
    int32_t GetNotFullPlaceholdersSize()const;

    inline void SetGuid(GUID_t nGuid);

    virtual GUID_t GetGuid()const override;
	
    void SetRemoveItemCallBack(remove_item_cbtype &cb)
    {
        m_oRemoveItemCb = cb;
    }

    inline void SetWaysOfObtaining(int32_t way) { m_oData.set_waysofobtaining(way); }
    inline int32_t GetWaysOfObtaining() { return m_oData.waysofobtaining();    }

    virtual int32_t GetCooldownGroup()const;


    int32_t GetCooldownTime()const;


	ItemData& GetItemData()
	{
		return m_oData;
	}

	const ItemData& GetItemData() const
	{
		return m_oData;
	}
#ifdef __TEST__
	void TestOutPut()const;

#endif // __TEST__
    float GetTotalEffectParam(int32_t nOverLaySize);
	

protected:
	const ItemProtoElement * m_pItemProtoElement;
	ItemData m_oData;
	check_capacity_cb_type m_bCheckCapacityCallback;
    remove_item_cbtype m_oRemoveItemCb;
    item_guid_callback_type item_change_callback_;
    item_guid_callback_type add_guid_item_callback_;
    get_equip_by_slottype m_oGetEquipBySlotType;
};

class CreateItemParam
{
public:


    typedef std::function<int32_t(std::size_t)> check_capacity_cb_type;
    typedef std::function<void(CreateItemParam &)> add_item_cb_type;


	int32_t m_nConfigId{ 1 };
	int32_t    m_vBind{ 1 };	
    bool m_bIsBindItem{ true };
	bool    m_vLock{ false };
    bool    m_bIsTransfer{false};
	int32_t m_nCount{ 1 };
    int32_t m_nOccupation{};

    IScriptHost * pPlayerScript{ NULL };

	int32_t nClassId{ -1 };
	int32_t nWayId{ -1 };
    add_item_cb_type m_oOnAddItemCB;
	int32_t m_nSoulStoneBuildId{ -1 };
	int32_t m_nAttackType{ 2 }; // 0.物理属性 1.魔法属性 2.通用
    int32_t m_nWaysOfObtaining{ 0 };
	bool m_bWild{ false }; //野生的  -- 宠物用
	bool m_bInTempBag{ true };	//是否进临时背包

    i32_v_type  m_vItemCountPairs;
	std::vector<std::shared_ptr<const Item>>* m_ItemCB{ 0 };
	std::list<ItemSimpleData>* m_pItemChangeCB{ nullptr };	//道具数量变化 --显示用
	bool m_loadfromdb{false};
    bool m_bAutoResolve{ true };
    const ItemData * m_pItemData{nullptr};
    int32_t m_nCD{ 0 };
    int32_t orginal_break_config_id{ 0 };

};

class CreateOutItemParam
{
public:

    struct AddInfo
    {
        int32_t confid_{ INVALID_ID };
        GUID_t item_guid_{ 0 };
        int32_t count_{ 0 };
    };
    using add_list_type = std::vector<AddInfo> ;

    std::vector<std::shared_ptr<const Item>>* m_ItemCB{ 0 };
	std::list<ItemSimpleData>* m_pItemChangeCB{ nullptr };		//道具数量变化 --显示用

    add_list_type add_list_;
};

enum
{
	E_UNBIND		= 0,
	E_PICKBIND		= 1,	//进背包
	E_EQUIPBIND		= 2,	//装备时绑定
};

class ItemFactory
{
public:
	typedef std::shared_ptr<Item> item_shared_ptr;
    typedef std::function<void(CreateItemParam &)> add_item_cb_type;
    typedef std::function<int32_t(int32_t)> i32_i32_callback_type;
    typedef std::function<int32_t(int32_t, int32_t, IScriptHost* )> float_player_i32_callback_type;

	static item_shared_ptr CreateItem(CreateItemParam & pCreateItemParam);
    static item_shared_ptr CreateItem(ItemData & pb, CreateItemParam & p);
	static void CreateItemData(ItemData & pb, int32_t nConfigid);

	static item_shared_ptr RandomItem();
    static int32_t RandomItemId();
	
   // static Snowflake & GetGuidGenertor();
	static GUID_t GenerateGuid();
	static void Init(uint64_t id);
    static void SetGetCurrencyCallBackType(const i32_i32_callback_type& get_item_type_callback);
    static void SetGetDropCanoverlay(const float_player_i32_callback_type& drop_item_canoverlay_callback);
    static bool AddCurrency(const CreateItemParam & pCreateItemParam);
	static bool            m_GuidInc;
	static nid_generator   m_item_ids;
	static bool            m_Inited;
    static i32_i32_callback_type get_item_type_callback_;
    static float_player_i32_callback_type drop_item_canoverlay_callback_;
private:
	
};
class BagBase;
class ItemParam
{
public:
    typedef std::function<int32_t(std::size_t)> check_capacity_cb_type;
    ItemParam(int32_t nPlayerLevel = 1,
        int32_t nUseSize = 1,
        int32_t nItemPos = 0,
        uint64_t nItemGuid = 0,
        bool vLock = false)
        : m_nPlayerLevel(nPlayerLevel),
        m_nUseSize(nUseSize),
        m_nItemPos(nItemPos),
        m_nItemGuid(nItemGuid),
        m_vLock(vLock),
        m_nClassId(-1),
        m_nWayId(-1),
        m_nConfigId(-1),
        item_type_(Item::E_EQUIPMENT)
    {

    }
    int32_t m_nPlayerLevel{ 1 };
    int32_t m_nUseSize{ 1 };
	int32_t m_nItemPos{ 0 };
    uint64_t m_nItemGuid{ 0 };
    bool    m_vLock{ false };
    IScriptHost * m_pPlayerScript{ NULL };
    check_capacity_cb_type m_cb;
    int32_t m_nClassId;
    int32_t m_nWayId;
    int32_t m_nConfigId;
    bool m_bClientRpc{ false };
    bool m_bBind{ true };
    IScriptHost * m_pTagetScript{ NULL };
    int32_t item_type_{ Item::E_EQUIPMENT };
	int32_t m_nOptional{ 0 };
    bool m_bClientBreak{false};
	BagBase* m_pBag{NULL};
};
} // namespace HumanBag

#endif // !ITEM_H_
