package deployservicelogic

import (
	"context"
	"fmt"
	"time"

	"deploy/internal/svc"
	"deploy/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type GetIDLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewGetIDLogic(ctx context.Context, svcCtx *svc.ServiceContext) *GetIDLogic {
	return &GetIDLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *GetIDLogic) GetID(in *game.GetIDRequest) (*game.GetIDResponse, error) {
	// todo: add your logic here and delete this line

	// 生成一个唯一 ID（在实际应用中，可以更复杂）
	id := fmt.Sprintf("%d", time.Now().UnixNano())

	// 将 ID 存入 ID 池，并设置 TTL
	s.idPool[id] = time.Now().Add(s.idTTL)
	fmt.Printf("Assigned ID: %s\n", id)

	return &idservice.GetIDResponse{
		Id: id,
	}, nil
	return &game.GetIDResponse{}, nil
}
