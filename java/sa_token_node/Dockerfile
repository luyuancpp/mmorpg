# ---------- build stage ----------
FROM maven:3.9-eclipse-temurin-23 AS build
WORKDIR /app
COPY pom.xml .
# Cache dependencies first
RUN mvn dependency:go-offline -B
COPY src src
RUN mvn package -DskipTests -B

# ---------- runtime stage ----------
FROM eclipse-temurin:23-jre
WORKDIR /app
COPY --from=build /app/target/*.jar app.jar
EXPOSE 5555 5556
ENTRYPOINT ["java", "-XX:+EnableDynamicAgentLoading", "-jar", "app.jar"]
