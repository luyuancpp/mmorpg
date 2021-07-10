#ifndef BAG_MODULE_H
#define BAG_MODULE_H



#include <memory>
#include <unordered_map>
#include <vector>
#include <unordered_set>

//#include "CommonLogic/GameEvent/EmidClass.h"

#include "StlDefineType.h"
#include "Item.h"

// #include "GenCode/Bag/BagRpc.pb.h"
// #include "GenCode/ItemOperation/ItemOperationRpc.pb.h"
// 
// 
// #include "GameDB.pb.h"

class Obj_Human;

class BagRpcClickItemAsk;
class BagRpcClickItemReply;
class BagRpcClearBagRedPointAsk;
class BagRpcClearBagRedPointReply;
class BagRpcUseItemAsk;
class BagRpcUseItemReply;
class BagRpcBagSyncAsk;
class BagRpcBagSyncReply;
class ItemOperationRpcItemResolveSettingSyncAsk;
class ItemOperationRpcItemResolveSettingSyncReply;
class BagRpcSaleItemAsk;
class BagRpcSaleItemReply;
class BagRpcLockItemAsk;
class BagRpcLockItemReply;
class BagRpcDeblockingAsk;
class BagRpcDeblockingReply;
class BagRpcTakeItemAsk;
class BagRpcTakeItemReply;
class BagRpcTidyAsk;
class BagRpcTidyReply;
class BagRpcTakeAllBackBagsAsk;
class BagRpcTakeAllBackBagsReply;
class BagRpcMergeItemAsk;
class BagRpcMergeItemReply;
class ItemOperationRpcItemResolveAsk;
class ItemOperationRpcItemResolveReply;
class ItemOperationRpcChangeSettingAsk;
class ItemOperationRpcChangeSettingReply;
class ItemOperationRpcBatchResolveAsk;
class ItemOperationRpcBatchResolveReply;

namespace HumanBag
{

    struct BreakInParam
    {
        int32_t auto_type_ = Item::E_EQUIPMENT;
    };

    struct BreakOutParam
    {
        i32_map_type break_item_map_;
        bool use_return_{ false };
    };

	class BagBase : public Emid
	{
	public:
		typedef std::shared_ptr<Item> item_type;
		typedef std::vector<item_type> item_list_type;
        typedef std::pair<GUID_t, int32_t> client_index_type;
        typedef std::vector<ItemObj> item_change_type;
		typedef std::vector<ItemObj> item_op_output_type;
        typedef std::unordered_map<int32_t, int32_t> pos_index_list_type;
        typedef std::function<bool(GUID_t)> bool_item_guid_callback_type;
        typedef std::function<void(GUID_t)> void_item_guid_callback_type;
        typedef std::vector<int32_t> config_list_type;

        typedef std::function<void()> void_cb_type;
        using get_equip_by_slottype = std::function<ItemFactory::item_shared_ptr(EquipTypeEnum::Type)>;
        typedef std::shared_ptr<BagBase> bag_ptr_type;

        static std::size_t g_nBagMaxSize;
        static std::size_t g_nTempBagMaxSize;
        static std::size_t g_nBagDefaultCapacity;
        static std::size_t g_nBagDeblockingSize;
        static std::size_t g_nStorageInitMaxSize;
        static std::size_t g_nStorageBagMaxSize;
        static std::size_t g_nPetBagMaxSize;
        static std::size_t g_nPetBagDeblockingSize;
        static std::size_t g_nItemMaxSize;
	public:
		enum BagTypeEnum
		{
			E_BAG_TYPE,
			E_BAG_STORAGE_TYPE,
			E_BAG_TEMP_TYPE,
			E_BAG_PET_TYPE,
			E_BAG_MAX_TYPE
		};

        static item_op_output_type kEmptyNewList;

		BagBase(int32_t nBagType);
        virtual ~BagBase() {}

        static bool InitStaticSize();

		virtual void OnLoad(const BagData &pb);
		virtual void OnSave(BagData &pb);

		virtual void OnNewDayCome() {};



		virtual void ToClientPb(BagData & pb);
		void ToClientItemPb(ItemObj & pb, int32_t nPos);
		void ToJson(Json::Value& bags);
		void ToClientItemPb(item_op_output_type & changeVec, int32_t nPos);
        void SetChangeItemPb(const item_type & pItem,
                             std::size_t pos_index);



		int32_t CheckClientPos(ItemParam &);
        int32_t CheckServerPos(ItemParam &);

		int32_t UseItem(ItemParam &);
        virtual int32_t OnUseItem(ItemParam &);
		virtual int32_t SaleItem(ItemParam &);
		virtual int32_t SetLock(ItemParam &);
        virtual int32_t SetBind(ItemParam &);

        /*virtual int32_t AddItem(item_type  & newItem,
            CreateItemParam & param);*/
        virtual int32_t AddItem(CreateItemParam & param, CreateOutItemParam& outparam);
		virtual int32_t AddItemForTest(CreateItemParam & param);
        int32_t AddItemFromOtherBag(item_type& new_item);
        void ResizeGrid();
        
        virtual int32_t SwapItem(int32_t nPos,  item_type  &  newItem);
		int32_t RemoveItem(ItemParam &);
        int32_t RemoveItemByConfigId(int32_t &configId, int32_t& nRemoveCount, ItemParam & p);
		int32_t ClearBag();


		int32_t TakeItem(ItemParam & p, item_type &outItem);
        int32_t TakeItem(int32_t nPos);
        virtual void ForwardItems() {}


		virtual int32_t GetAllItem(item_list_type & v);
        virtual int32_t TakeAllItem(item_list_type & v);
		//解锁
		virtual int32_t Deblocking();

		virtual bool IsFull();

		int32_t MergeItem(ItemParam &);
		int32_t MergeItem(item_list_type::iterator & lower, item_list_type::iterator & upper,bool bMergeDifCd = false);


		//整理
		int32_t Tidy();

		int32_t EnoughCapacity(std::size_t nSize)const;

        
        int32_t CanOverlayItem(const i32_map_type & oConfigList);
        int32_t CanOverlayItem(const i32_map_type & oConfigList, CreateItemParam& param);
        int32_t CanOverlayItem(int32_t nConfigId, int32_t nOverlaySize);
        int32_t CanOverlayBreakItem(item_type pItem, i32_map_type & oConfigList, int32_t nOverlaySize);
        int32_t CanOverlayBreakItem(i32_v_type & pItemList, i32_map_type & oConfigList);
        virtual  int32_t CanOverlayItemFullErro();

        virtual int32_t BreakItem(ItemParam & p, i32_map_type  &oConf, bag_ptr_type pBag = bag_ptr_type());
        virtual int32_t BreakItem(ItemParam & p, int32_t nPos, i32_map_type  &oConf, bag_ptr_type pBag = bag_ptr_type());
        virtual int32_t BreakItem(ItemParam & p, i32_v_type & pItemList, bag_ptr_type pBag = bag_ptr_type());

        int32_t CheckEnoughItem(i32_map_type & oConsumeList, pos_index_list_type &outindexlist);

        int32_t SaleBreakAllItem(int32_t nQuality, ItemParam & player, BreakInParam& intparam, BreakOutParam& outparam);

       
        
		std::size_t GetCurCapacity()const
		{
			return m_nCurCapacity;
		}

		inline std::size_t GetItemSize()const
		{
			return m_vItemList.size() - CalcReturnEmptySize();
		}




		std::size_t GetEmptySize()const;
        std::size_t GetTailEmptySize()const;
		int32_t GetOverlaySize();
        int32_t GetOverlaySize(int32_t nPos);
        bool CanAddSize(std::size_t gridSize);
        int32_t CanAddItem(item_type & newItem);

		int32_t GetItemCount(int32_t nItemID);

		item_list_type GetSameItemList(item_type & newItem,bool bMergeDifCd,bool bMergeBind);
		item_type& GetItem(int32_t nPos);
        item_type GetItemByGuid(GUID_t nGuid);
		item_type GetItemByGuid(GUID_t nGuid,int32_t& nPos);
        int32_t GetItemPos(GUID_t nGuid);
        bool HasItem(GUID_t nGuid);
        float GetItemTotalEffectParam(int32_t nPos);
        float GetItemTotalEffectParam(int32_t nPos, int32_t nOverLaySize);

		static bool ItemSortFn(const item_type & lhs, const item_type & rhs);

		std::size_t CalcReturnEmptySize()const;

        int32_t UseItem(pos_index_list_type & configs, ItemParam &p);
#ifdef __TEST__
		void TestOutPut();

        int32_t RandomItemPos();
#endif // __TEST__

		virtual int32_t GetFullError();

        void SetHuman(Obj_Human * pHuman)
        {
            m_pHuman = pHuman;
        }

 

        void NotifyItemChange();


        const item_change_type & GetChangedItem() { return client_new_changed_vec_; }

        void OnNotify();


        void SetReadPoint(const bool_item_guid_callback_type & oHasRedPoint)
        {
            m_oHasRedPoint = oHasRedPoint;
        }

        void SetAddGuid(const void_item_guid_callback_type & check_callback)
        {
            add_guid_item_callback_ = check_callback;
        }
        

        void SetAutoSaleCB(const void_cb_type & oAutoSaleCB)
        {
            m_oAutoSaleCB = oAutoSaleCB;
        }

        void SetGetEquipBySlotType(const get_equip_by_slottype& oCB)
        {
            m_oGetEquipBySlotType = oCB;
        }

        const get_equip_by_slottype& GetGetEquipBySlotTypeCallBack() const
        {
            return m_oGetEquipBySlotType;
        }

        void SetClearRedPoint(const void_cb_type & clear_read_point_callback)
        {
            clear_read_point_callback_ = clear_read_point_callback;
        }
        
        int32_t GetBagType()const { return m_nBagType; }

        void OnItemChange(ItemParam &p);
        void OnItemChange(int32_t item_pos);
        void OnItemChangeFromGuid(GUID_t item_guid);
  
        void OnAllChange();
        void ClearChangeItem();


        static int32_t IsOpenFunctionItem(int32_t item_id);

protected:
    item_type CreateNullItem();
    item_type FillItem(CreateItemParam & pCreateItemParam);
    item_type CreateItem(CreateItemParam & pCreateItemParam);
    void OnAddNewGridItemSucces(CreateItemParam & cip, CreateOutItemParam& outparam, BagBase::item_type& p_new_item);
	void OnAddOverlaydItemSucces(CreateItemParam & cip, CreateOutItemParam& outparam, BagBase::item_type& p_new_item, int32_t add_count);
    inline int32_t CanOverlayItem(item_type & newItem, item_type & curItem);
    inline int32_t CanOverlayItem(item_type & curItem, CreateItemParam& param);
    int32_t TryBreakSale(item_type & pItem);
protected:
		void UpdateUpper(item_list_type::iterator & lower, item_list_type::iterator & upper);
        void InitItemCallBack(item_type&  newItem);
	protected:
		item_list_type m_vItemList;
        std::size_t m_nCurCapacity{0};

		int32_t m_nBagType;

        Obj_Human * m_pHuman{NULL};

        bool_item_guid_callback_type m_oHasRedPoint;
        void_item_guid_callback_type add_guid_item_callback_;
        void_cb_type m_oAutoSaleCB;
        void_cb_type clear_read_point_callback_;
        get_equip_by_slottype m_oGetEquipBySlotType;
        BagBase::item_change_type client_new_changed_vec_;

        bool showquickuseui_{false};
	};

    class BagModule : public Emid
	{
	public:
		typedef std::shared_ptr<BagBase> bag_ptr_type;
		typedef std::unordered_map<int32_t, bag_ptr_type> bag_list_type;
		typedef std::unordered_set<int32_t> can_put_into_type;
		typedef std::unordered_map<int32_t, can_put_into_type> put_type;
        typedef std::function<int32_t()> get_occuption_cb_type;
        typedef typename BagBase::pos_index_list_type pos_index_list_type;
        typedef std::unordered_set<GUID_t> item_guid_set_type;
		typedef std::function<bool(HumanBag::BagBase::item_type&)> pet_autobreak_type;
        typedef std::map<int32_t , ResolveUnit> resolve_unit_type;

#ifndef __TEST__
        BagModule(Obj_Human * pHuman, Emid::manager_id player_em_id)
            : Emid(player_em_id),
              m_pHuman(pHuman)

        {
            Init();
        }

#endif // !__TEST__

		BagModule();

        BagModule(const BagModule& o) = delete;
        BagModule& operator=(const BagModule& o) = delete;


        void SetGetOccupationCB(const get_occuption_cb_type & oOccupation)
        {
            m_oOccupation = oOccupation;
        }

		void SetPetAutoBreakCB(const pet_autobreak_type& oCB)
		{
			m_oPetAutoBreakCB = oCB;
		}

        void SetGetEquipBySlotType(const BagBase::get_equip_by_slottype& oCB);



        void OnLoad(const DBBags & pb);
        void OnSave(DBBags & pb);
		void OnSave(user& db, user& dbcache);
		void OnLoadItemAutoOperation(const ItemResolveDataList& pb);
		void OnSaveItemAutoOperation(ItemResolveDataList& pb);
		void OnSaveItemAutoOperation(user& db, user& dbcache);

		void OnNewDayCome();

        int32_t CreateItem(ItemData & pb, CreateItemParam &p);
        int32_t CreateItem(CreateItemParam & p);
        int32_t CreateItem(ItemData & pb, CreateItemParam &p, CreateOutItemParam& outparam);
        int32_t CreateItem(CreateItemParam & p, CreateOutItemParam& outparam);
		int32_t SafeCreateItem(CreateItemParam& p,  CreateOutItemParam& outparam );
		bag_ptr_type  GetBag(int32_t nBagType);
		bag_ptr_type  GetBagByItem(BagBase::item_type pItem);
        bag_ptr_type  GetBagByItemConfig(int32_t n_config_id);
		bag_ptr_type  GetBagByItemId(int32_t nConfigId);
		void ToClientPb(::google::protobuf::RepeatedPtrField< ::BagData > & pb);
		void ToJson(Json::Value& bags);
		int32_t TakeAllItemFromTempBag();
		int32_t TakeItem(int32_t nFromBagType, int32_t nToBagType, ItemParam & p);
        int32_t SwapItem(int32_t nPos, BagBase::item_type & newItem);
        BagBase::item_type& GetItem(int32_t nFromBagType, int32_t nPos);
        int32_t SaleBreakAllItem(int32_t nQuality,  BreakInParam& intparam, BreakOutParam& outparam);
        int32_t SaleBreakAllItem(int32_t nQuality);
        int32_t SaleBreakAllItemNoParam();
        int32_t SetItemAutoOperatorData(ItemAutoOperatorData & pb, int32_t auto_type = Item::E_EQUIPMENT);

		int32_t ClearBag();

        // not to add to temp bag

        int32_t BreakItem(ItemParam & p);
        int32_t GetItemCount(int32_t configid);

        float GetConsumeItemTotalEffectParam(BagBase::pos_index_list_type outindexlist);
        int32_t GetItemSubType(BagBase::pos_index_list_type outindexlist);

        int32_t CheckEnoughItem(i32_map_type & oConsumeList, BagBase::pos_index_list_type &outindexlist);
		//检测使用多个背包道具的
		int32_t CheckEnoughItem(i32_map_type& oConsumeList, std::map<int32_t, BagBase::pos_index_list_type>& outindexlistWithBag);
        int32_t RemoveItem(HumanBag::ItemParam & p, int32_t nBagType = BagBase::E_BAG_TYPE);
        int32_t RemoveItemNotifyClient(HumanBag::ItemParam & p, int32_t nBagType = BagBase::E_BAG_TYPE);
        int32_t RemoveItemNotifyClient(BagBase::pos_index_list_type &outindexlist, HumanBag::ItemParam & p, int32_t nBagType = BagBase::E_BAG_TYPE);
        int32_t RemoveItem(BagBase::pos_index_list_type &outindexlist, HumanBag::ItemParam & p, int32_t nBagType = BagBase::E_BAG_TYPE);
		//删除使用多个背包道具的
		int32_t RemoveItem(std::map<int32_t, BagBase::pos_index_list_type>& outindexWithBag, HumanBag::ItemParam& param);

        int32_t BreakItemByPos(BagBase::pos_index_list_type &outindexlist, int32_t nBagType = BagBase::E_BAG_TYPE);
        int32_t RemoveItemByConfigId(int32_t configId, int32_t& nRemoveCount, HumanBag::ItemParam & p, int32_t nBagType = BagBase::E_BAG_TYPE);

        int32_t CheckAndRemove(i32_map_type & oConsumeList, HumanBag::ItemParam  & checkp);
        int32_t UseItem(i32_map_type & v,  ItemParam &p);
        int32_t UseItemCallback(i32_map_type & v, ItemParam &p);

		//client rpc function
        void RpcClickItem(BagRpcClickItemAsk & rq, BagRpcClickItemReply & rsp, ItemParam & p);
        void RpcClearReadPoint(BagRpcClearBagRedPointAsk & rq, BagRpcClearBagRedPointReply & rsp);
		void RpcUseItem(BagRpcUseItemAsk & rq, BagRpcUseItemReply & rsp, ItemParam & p);
		void RpcSync(BagRpcBagSyncAsk & rq, BagRpcBagSyncReply & rsp);
        void RpcSync(ItemOperationRpcItemResolveSettingSyncAsk & rq, ItemOperationRpcItemResolveSettingSyncReply & rsp);
		void RpcSaleItem(BagRpcSaleItemAsk & rq, BagRpcSaleItemReply & rsp, ItemParam & p);
		void RpcLockItem(BagRpcLockItemAsk &rq, BagRpcLockItemReply & rsp, ItemParam & p);
		void RpcDeblocking(BagRpcDeblockingAsk & rq, BagRpcDeblockingReply & rsp);
		void RpcTakeItem(BagRpcTakeItemAsk &rq, BagRpcTakeItemReply &rsp);
		void RpcTidy(BagRpcTidyAsk &rq, BagRpcTidyReply &rsp);
		void RpcGetAllItemFromTempBag(BagRpcTakeAllBackBagsAsk &rq, BagRpcTakeAllBackBagsReply&rsp);
		void RpcMergeItem(BagRpcMergeItemAsk &rq, BagRpcMergeItemReply&rsp);
        void RpcItemResolve(ItemOperationRpcItemResolveAsk &rq, ItemOperationRpcItemResolveReply&rsp);
        void RpcChangeSetting(ItemOperationRpcChangeSettingAsk &rq, ItemOperationRpcChangeSettingReply&rsp);
        void RpcBatchResolve(ItemOperationRpcBatchResolveAsk &rq, ItemOperationRpcBatchResolveReply&rsp);

       
        ItemObj GetNewOneAddedOrChangeItem();

        void DoAllChangeAndNotifyClient();
        void NotifyClientBagChangeMsg();
        void ClearChangeItem();
     
        int32_t CanOverlayItem(const i32_map_type & oConfigList, CreateItemParam& param);
        int32_t CanOverlayItem(const i32_map_type & oConfigList);

        bool AutoSaleDisassemble(CreateItemParam & cip, CreateOutItemParam& outparam );
       
        void SetAllBagNotChnage();


        bool IsBagFull();
        void Tidy();

        std::size_t GetBagCurCapacity(int32_t bagType);

        void SetBagAllChange();

        int32_t EnoughCapacity(std::size_t nSize, int32_t bag_type = BagBase::E_BAG_TYPE);

    private:
       
        void OnRemoveItem(const ItemParam& param);
        void Init();
        void InitResolvePb(int32_t equip_type);
        bool HasRedPoint(GUID_t nGuid);
        void AddRedPoint(GUID_t nGuid);
        void ClearRedPoint();

        bool CheckAutoBreak(BagBase::item_type & pItem);
        bool CheckAutoSaleDisassembleFilter(BagBase::item_type & pItem);
      
        

        const BagBase::item_change_type & GetNewAddItem();
        const BagBase::item_change_type & GetChangedItem();
        const ItemObj GetFirstChangeItem();
		std::map<int32_t, i32_map_type> MakeConsumeListWithBagType(const i32_map_type& oConsumeList) const;
    private:
            int32_t CreateItemEx(const CreateItemParam & inparam, CreateOutItemParam& outparam);
            void UsePickUpTypeItem(CreateOutItemParam& outparam);
	private:
		bag_list_type m_vBags;
		put_type m_Puts;
        ItemAutoOperatorData m_oAutoItemOpData;
        get_occuption_cb_type m_oOccupation;

        Obj_Human * m_pHuman{ nullptr };

		pet_autobreak_type m_oPetAutoBreakCB;
        item_guid_set_type m_vRedGuids;
        resolve_unit_type resolve_unit_;
        static const int32_t kMaxResolveSize{ 3 };

	};
    
} // namespace HumanBag

#endif // !BAG_MODULE_H
