#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "config_expression/config_expression.h"
#include "muduo/base/Logging.h"
#include "buff_config.pb.h"

class BuffConfigurationTable {
public:
    using KeyValueDataType = std::<uint32_t, const BuffTable*>;

    static BuffConfigurationTable& Instance() {
        static BuffConfigurationTable instance;
        return instance;
    }

    const BuffTabledData& All() const { return data_; }
    std::pair<const BuffTable*, uint32_t> GetTable(uint32_t tableId);
    std::pair<const BuffTable*, uint32_t> GetTableWithoutErrorLogging(uint32_t tableId);
    const KeyValueDataType& KeyValueData() const { return kv_data_; }
    void Load();

    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
                double GetHealthregeneration(const uint32_t tableId);
                void SetHealthregenerationParam(const std::vector<double>& paramList);
            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
                double GetBonusdamage(const uint32_t tableId);
                void SetBonusdamageParam(const std::vector<double>& paramList);
            
        
    

private:
    BuffTabledData data_;
    KeyValueDataType kv_data_;

    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
                ExcelExpression<double> expression_healthregeneration_;
            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
        
    
        
            

            
                ExcelExpression<double> expression_bonusdamage_;
            
        
    
};

inline const BuffTabledData& GetBuffAllTable() {
    return BuffConfigurationTable::Instance().All();
}

#define FetchAndValidateBuffTable(tableId) \\
    const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(tableId); \\
    do { if (!( buffTable )) { LOG_ERROR << "Buff table not found for ID: " << tableId; return fetchResult; } } while(0)

#define FetchAndValidateCustomBuffTable(prefix, tableId) \\
    const auto [##prefix##BuffTable, prefix##fetchResult] = BuffConfigurationTable::Instance().GetTable(tableId); \\
    do { if (!(##prefix##BuffTable)) { LOG_ERROR << "Buff table not found for ID: " << tableId; return prefix##fetchResult; } } while(0)

#define FetchBuffTableOrReturnCustom(tableId, customReturnValue) \\
    const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(tableId); \\
    do { if (!( buffTable )) { LOG_ERROR << "Buff table not found for ID: " << tableId; return customReturnValue; } } while(0)

#define FetchBuffTableOrReturnVoid(tableId) \\
    const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(tableId); \\
    do { if (!( buffTable )) { LOG_ERROR << "Buff table not found for ID: " << tableId; return; } } while(0)

#define FetchBuffTableOrContinue(tableId) \\
    const auto [buffTable, fetchResult] = BuffConfigurationTable::Instance().GetTable(tableId); \\
    do { if (!( buffTable )) { LOG_ERROR << "Buff table not found for ID: " << tableId; continue; } } while(0)

#define FetchBuffTableOrReturnFalse(tableId) \\
    const auto [buffTable, f]()