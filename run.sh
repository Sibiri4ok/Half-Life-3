#!/bin/bash

# Проверка наличия build директории
if [ ! -d "build" ]; then
    mkdir -p build
fi

# Проверка, нужна ли пересборка
NEEDS_REBUILD=false

if [ ! -f "bin/main" ]; then
    NEEDS_REBUILD=true
else
    # Проверка изменений в исходниках
    if [ "engine/ecs/systems.cpp" -nt "bin/main" ] || \
       [ "engine/ecs/components.h" -nt "bin/main" ] || \
       [ "game/loops/game_loop.cpp" -nt "bin/main" ]; then
        NEEDS_REBUILD=true
    fi
fi

if [ "$NEEDS_REBUILD" = true ]; then
    ./scripts/build.sh --release --without-tests
    
    if [ $? -ne 0 ]; then
        exit 1
    fi
else
    echo "✅ Используется существующая сборка"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "🚀 Запуск игры..."
echo ""
echo "💡 Подсказка: Стреляйте в направлении последнего движения!"
echo ""
sleep 1

./scripts/play.sh



