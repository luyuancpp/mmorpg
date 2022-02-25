import os

import buildrpcservicetool

buildrpcservicetool.generate('gw2l.proto', '../../../../login_server/src/service')
buildrpcservicetool.generate('l2db.proto', '../../../../database_server/src/service')
buildrpcservicetool.generate('ms2g.proto', '../../../../game_server/src/service')
buildrpcservicetool.generate('rg2g.proto', '../../../../game_server/src/service')
buildrpcservicetool.generate('node2deploy.proto', '../../../../deploy_server/src/service')
buildrpcservicetool.generate('ms2gw.proto', '../../../../gateway_server/src/service')
buildrpcservicetool.generate('gw2ms.proto', '../../../../master_server/src/service')
buildrpcservicetool.generate('l2ms.proto', '../../../../master_server/src/service')